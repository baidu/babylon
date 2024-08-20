#pragma once

#include "babylon/absl_numeric_bits.h" // absl::bit_ceil
#include "babylon/concurrent/transient_hash_table.h"
#include "babylon/reusable/allocator.h" // UsesAllocatorConstructor

#ifdef __x86_64__
#include <x86intrin.h>
#endif // __x86_64__

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

#if ABSL_HAVE_THREAD_SANITIZER
#include <sanitizer/tsan_interface.h> // ::__tsan_acquire
#endif                                // ABSL_HAVE_THREAD_SANITIZER

#pragma GCC diagnostic push
// Thread Sanitizer目前无法支持std::atomic_thread_fence
// gcc-12之后增加了相应的报错，显示标记忽视并特殊处理相关段落
#if GCC_VERSION >= 120000
#pragma GCC diagnostic ignored "-Wtsan"
#endif // GCC_VERSION >= 120000

BABYLON_NAMESPACE_BEGIN

namespace internal {
namespace concurrent_transient_hash_table {

////////////////////////////////////////////////////////////////////////////////
// 从存储的value中提取key值的插件点
// key = Extractor::extract(value)

// 支持set类型
// value = key
struct IdentityKeyExtractor {
  template <typename T>
  inline static const T& extract(const T& value) noexcept {
    return value;
  }
};

// 支持map类型
// value = std::pair<key, mapped>
template <typename K, typename V>
struct PairKeyExtractor {
  inline static const K& extract(const ::std::pair<K, V>& value) noexcept {
    return value.first;
  }
  inline static const K& extract(
      const ::std::pair<const K, V>& value) noexcept {
    return value.first;
  }
  inline static const K& extract(
      const ::std::pair<K, const V>& value) noexcept {
    return value.first;
  }
  inline static const K& extract(
      const ::std::pair<const K, const V>& value) noexcept {
    return value.first;
  }
  template <typename T,
            typename = typename ::std::enable_if<!::std::is_convertible<
                T, const ::std::pair<const K, V>&>::value>::type>
  inline static T&& extract(T&& value) noexcept {
    return ::std::forward<T>(value);
  }
};
////////////////////////////////////////////////////////////////////////////////

// 基于64位bitmask的迭代器，用于解读SIMD匹配的结果
// 在x86上SIMD指令产出的mask每个bit表示一个匹配结果
// for (int i : GroupIterator(0b101)) -> yields 0, 2
// 在ARM上SIMD指令产出的mask每4个bit表示一个匹配结果
// for (int i : GroupIterator(0xF0F00)) -> yields 2, 4
class GroupIterator {
 public:
  // 一般来说迭代器的自增函数需要返回自增前的本体
  // 而这个本体一般又需要通过operator*来取值
  //
  // 对于GroupIterator而言，取值操作也是一个bitmask操作，有一定开销
  // 另一方面，自增过程中其实可以顺便计算出这个取值结果
  // 因此通过包装这个取值结果支持operator*来轻量级直接返回
  // 可以有效避免这次额外的bitmask操作开销
  //
  // 最终使用时，也能提供*iter和*iter++
  // 这两种比较容易理解的写法
  class GroupIndex {
   public:
    inline GroupIndex(size_t index) noexcept : _index {index} {}

    inline size_t operator*() const noexcept {
      return _index;
    }

   private:
    size_t _index;
  };

  inline GroupIterator() noexcept = default;

  inline explicit GroupIterator(uint64_t mask) noexcept : _mask {mask} {}

  inline explicit operator bool() const noexcept {
    return _mask != 0;
  }

  inline size_t operator*() noexcept {
#if defined(__ARM_NEON)
    return static_cast<size_t>(__builtin_ctzll(_mask) >> 2);
#else  // !defined(__ARM_NEON)
    return static_cast<size_t>(__builtin_ctzll(_mask));
#endif // !defined(__ARM_NEON)
  }

  inline ABSL_ATTRIBUTE_ALWAYS_INLINE GroupIndex operator++(int) noexcept {
#if defined(__ARM_NEON)
    auto offset = static_cast<size_t>(__builtin_ctzll(_mask));
    _mask = _mask - (static_cast<uint64_t>(0xF) << offset);
    return offset >> 2;
#else  // !defined(__ARM_NEON)
    auto offset = static_cast<size_t>(__builtin_ctzll(_mask));
    _mask = _mask - (0x1 << offset);
    return offset;
#endif // !defined(__ARM_NEON)
  }

 private:
  uint64_t _mask {0};
};

// 采用SIMD进行一个组控制位快速检测的方法，根据平台采用不同的SIMD实现
// 检测结果采用bitmask呈现，通过GroupIterator来解读
// 组大小目前固定为16个byte
class Group {
 public:
  static constexpr size_t SIZE = 16;
  static constexpr size_t GROUP_MASK = 0xF;
  static constexpr size_t GROUP_MASK_BITS = 4;
  static constexpr size_t CHECKER_MASK = 0x7F;
  static constexpr size_t CHECKER_MASK_BITS = 7;

  // 默认构造后的特殊状态，不存在元素，但又不可写入
  static constexpr int8_t DUMMY_CONTROL = static_cast<int8_t>(0x82);
  // 元素正在写入中
  static constexpr int8_t BUSY_CONTROL = static_cast<int8_t>(0x81);
  // 可用未知当前也没有写入过元素
  static constexpr int8_t EMPTY_CONTROL = static_cast<int8_t>(0x80);

  static_assert(sizeof(int8_t) == sizeof(::std::atomic<int8_t>) &&
                    alignof(int8_t) == alignof(::std::atomic<int8_t>),
                "atomic int need same to int");

  inline ABSL_ATTRIBUTE_ALWAYS_INLINE Group(
      ::std::atomic<int8_t>* controls) noexcept;

  // 检测16个byte各自是否和check完全一致
  // check为hash结果后7bit，用于快速粗筛
  inline ABSL_ATTRIBUTE_ALWAYS_INLINE GroupIterator
  match(int8_t check) noexcept {
#if defined(__SSE2__)
    uint16_t mask =
        _mm_movemask_epi8(_mm_cmpeq_epi8(_controls, _mm_set1_epi8(check)));
#elif defined(__ARM_NEON)
    uint16x8_t mask128 =
        vreinterpretq_u16_u8(vceqq_s8(_controls, vdupq_n_s8(check)));
    uint8x8_t mask64 = vshrn_n_u16(mask128, 4);
    uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(mask64), 0);
#else  // !defined(__SSE2__) && !defined(__ARM_NEON)
    uint16_t mask = 0;
    for (size_t i = 0; i < SIZE; ++i) {
      if (check == _controls[i]) {
        mask |= 1 << i;
      }
    }
#endif // !defined(__SSE2__) && !defined(__ARM_NEON)
    return GroupIterator {mask};
  }

  // 检测16个byte各自是否表示有内容
  // 有内容的byte都采用7bit表达，通过符号位判定即可快速过滤
  // 符号位为1的特殊值，在查找时都视为『空』
  // 尽管emplace操作并不认为可以发起写入
  inline ABSL_ATTRIBUTE_ALWAYS_INLINE GroupIterator match_empty() noexcept {
#if defined(__SSE2__)
    uint16_t mask = _mm_movemask_epi8(_controls);
#elif defined(__ARM_NEON)
    uint16x8_t mask128 = vreinterpretq_u16_u8(
        vceqq_s8(vandq_s8(_controls, vdupq_n_s8(0x80)), vdupq_n_s8(0x80)));
    uint8x8_t mask64 = vshrn_n_u16(mask128, 4);
    uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(mask64), 0);
#else  // !defined(__SSE2__) && !defined(__ARM_NEON)
    uint16_t mask = 0;
    for (size_t i = 0; i < SIZE; ++i) {
      if (_controls[i] < 0) {
        mask |= 1 << i;
      }
    }
#endif // !defined(__SSE2__) && !defined(__ARM_NEON)
    return GroupIterator {mask};
  }

  // match_empty的取反结果，用于遍历有内容的位置
  inline ABSL_ATTRIBUTE_ALWAYS_INLINE GroupIterator match_non_empty() noexcept {
#if defined(__SSE2__)
    uint16_t mask = ~_mm_movemask_epi8(_controls);
#elif defined(__ARM_NEON)
    uint16x8_t mask128 = vreinterpretq_u16_u8(
        vceqq_s8(vandq_s8(_controls, vdupq_n_s8(0x80)), vdupq_n_s8(0x00)));
    uint8x8_t mask64 = vshrn_n_u16(mask128, 4);
    uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(mask64), 0);
#else  // !defined(__SSE2__) && !defined(__ARM_NEON)
    uint16_t mask = 0;
    for (size_t i = 0; i < SIZE; ++i) {
      if (_controls[i] >= 0) {
        mask |= 1 << i;
      }
    }
#endif // !defined(__SSE2__) && !defined(__ARM_NEON)
    return GroupIterator {mask};
  }

  static void clear(::std::atomic<int8_t>* controls) noexcept {
#if defined(__SSE2__)
    _mm_store_si128(reinterpret_cast<__m128i*>(controls),
                    _mm_set1_epi8(EMPTY_CONTROL));
#elif defined(__ARM_NEON)
    vst1q_s8(reinterpret_cast<int8_t*>(controls), vdupq_n_s8(EMPTY_CONTROL));
#else  // !defined(__SSE2__) && !defined(__ARM_NEON)
    __builtin_memset(reinterpret_cast<void*>(controls), EMPTY_CONTROL, SIZE);
#endif // !defined(__SSE2__) && !defined(__ARM_NEON)
  }

  static ::std::atomic<int8_t> s_dummy_controls[];

 private:
#if defined(__SSE2__)
  __m128i _controls;
#elif defined(__ARM_NEON)
  int8x16_t _controls;
#else  // !defined(__SSE2__) && !defined(__ARM_NEON)
  int8_t _controls[16];
#endif // !defined(__SSE2__) && !defined(__ARM_NEON)
};
static_assert(sizeof(Group) == Group::SIZE, "group struct size invalid");

inline ABSL_ATTRIBUTE_ALWAYS_INLINE Group::Group(
    ::std::atomic<int8_t>* controls) noexcept {
// 对x86和aarch64体系架构而言
// relaxed atomic操作和non atomic操作使用相同指令完成
// 使用atomic_thread_fence可以实现SIMD load和atomic release配对
//
// 但因为这并不是ISO规范中能够跨平台支持的特性
// Thread Sanitizer实现上限定了acquire/release操作需要atomic配对
// 针对性增加了一次atomic转存让Thread Sanitizer可识别
// 采用relaxed等级确保fence正确性依然能够得到验证
#if ABSL_HAVE_THREAD_SANITIZER
  int8_t vec[SIZE];
  for (size_t i = 0; i < SIZE; ++i) {
    vec[i] = controls[i].load(::std::memory_order_relaxed);
  }
#else  // ABSL_HAVE_THREAD_SANITIZER
  auto& vec = controls;
#endif // ABSL_HAVE_THREAD_SANITIZER
#if defined(__SSE2__)
  _controls = _mm_loadu_si128(reinterpret_cast<__m128i*>(vec));
#elif defined(__ARM_NEON)
  _controls = vld1q_s8(reinterpret_cast<int8_t*>(vec));
#else  // !defined(__SSE2__) && !defined(__ARM_NEON)
  __builtin_memcpy(_controls, vec, SIZE);
#endif // !defined(__SSE2__) && !defined(__ARM_NEON)
}

} // namespace concurrent_transient_hash_table
} // namespace internal

// ConcurrentFixedSwissTable配套的迭代器
template <typename T, typename H, typename E>
template <bool CONST>
class ConcurrentFixedSwissTable<T, H, E>::Iterator {
 public:
  using difference_type = ssize_t;
  using value_type = ConcurrentFixedSwissTable::value_type;
  using pointer = typename ::std::conditional<CONST, const T*, T*>::type;
  using reference = typename ::std::conditional<CONST, const T&, T&>::type;
  using iterator_category = ::std::input_iterator_tag;

  inline Iterator() noexcept = default;
  inline Iterator(Iterator&&) noexcept = default;
  inline Iterator(const Iterator&) noexcept = default;
  inline Iterator& operator=(Iterator&&) noexcept = default;
  inline Iterator& operator=(const Iterator&) noexcept = default;
  inline ~Iterator() noexcept = default;

  template <bool OTHER_CONST,
            typename = typename ::std::enable_if<CONST || !OTHER_CONST>::type>
  inline Iterator(const Iterator<OTHER_CONST>& other) noexcept;

  inline reference operator*() const noexcept;
  inline pointer operator->() const noexcept;
  inline bool operator==(Iterator other) const noexcept;
  inline bool operator!=(Iterator other) const noexcept;

  inline Iterator& operator++() noexcept;

  // 特殊接口，用于判定是否已经结束迭代
  // 等同于iter != table.end()
  // 但是因为迭代器实现上自带table
  // 可以简化包装层避免再存储一次table指针
  inline operator bool() const noexcept;

 private:
  using GroupIterator =
      internal::concurrent_transient_hash_table::GroupIterator;

  inline Iterator(const ConcurrentFixedSwissTable& table,
                  size_t index) noexcept;
  inline Iterator(const ConcurrentFixedSwissTable& table, size_t index,
                  GroupIterator) noexcept;

  ConcurrentFixedSwissTable* _table {nullptr};
  size_t _index {SIZE_MAX};
  GroupIterator _iter;

  friend class ConcurrentFixedSwissTable;
};

template <typename T, typename H, typename E>
template <bool CONST>
class ConcurrentTransientHashSet<T, H, E>::Iterator {
 private:
  using Table = typename ConcurrentTransientHashSet::Table;
  using TableIterator = typename Table::template Iterator<CONST>;

 public:
  using difference_type = ssize_t;
  using value_type = ConcurrentTransientHashSet::value_type;
  using pointer = typename ::std::conditional<CONST, const T*, T*>::type;
  using reference = typename ::std::conditional<CONST, const T&, T&>::type;
  using iterator_category = ::std::input_iterator_tag;

  inline Iterator() noexcept = default;
  inline Iterator(Iterator&&) noexcept = default;
  inline Iterator(const Iterator&) noexcept = default;
  inline Iterator& operator=(Iterator&&) noexcept = default;
  inline Iterator& operator=(const Iterator&) noexcept = default;
  inline ~Iterator() noexcept = default;

  template <bool OTHER_CONST,
            typename = typename ::std::enable_if<CONST || !OTHER_CONST>::type>
  inline Iterator(const Iterator<OTHER_CONST>& other) noexcept;

  inline reference operator*() const noexcept;
  inline pointer operator->() const noexcept;
  inline bool operator==(Iterator other) const noexcept;
  inline bool operator!=(Iterator other) const noexcept;

  inline Iterator& operator++() noexcept;

 private:
  inline Iterator(TableNode* next, TableIterator iter) noexcept;

  TableNode* _next {nullptr};
  TableIterator _iter;

  friend class ConcurrentTransientHashSet;
};

////////////////////////////////////////////////////////////////////////////////
// ConcurrentFixedSwissTable::Iterator begin
template <typename T, typename H, typename E>
template <bool CONST>
template <bool OTHER_CONST, typename>
inline ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::Iterator(
    const Iterator<OTHER_CONST>& other) noexcept
    : Iterator(*other._table, other._index, other._iter) {}

template <typename T, typename H, typename E>
template <bool CONST>
inline
    typename ConcurrentFixedSwissTable<T, H,
                                       E>::template Iterator<CONST>::reference
    ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::operator*()
        const noexcept {
  return _table->at(_index);
}

template <typename T, typename H, typename E>
template <bool CONST>
inline typename ConcurrentFixedSwissTable<T, H,
                                          E>::template Iterator<CONST>::pointer
ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::operator->()
    const noexcept {
  return &_table->at(_index);
}

template <typename T, typename H, typename E>
template <bool CONST>
inline bool ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::operator==(
    Iterator other) const noexcept {
  return _index == other._index;
}

template <typename T, typename H, typename E>
template <bool CONST>
inline bool ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::operator!=(
    Iterator other) const noexcept {
  return !(*this == other);
}

template <typename T, typename H, typename E>
template <bool CONST>
inline typename ConcurrentFixedSwissTable<T, H, E>::template Iterator<CONST>&
ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::operator++() noexcept {
  size_t base_index = _index & ~Group::GROUP_MASK;
  if (!_iter) {
    auto base_index_and_iter =
        _table->find_first_non_empty(base_index + Group::SIZE);
    base_index = ::std::get<0>(base_index_and_iter);
    _iter = ::std::get<1>(base_index_and_iter);
    if (!_iter) {
      _index = base_index;
      return *this;
    }
  }
  size_t offset = *_iter++;
  _index = base_index + offset;
  return *this;
}

template <typename T, typename H, typename E>
template <bool CONST>
inline ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::operator bool()
    const noexcept {
  // 用<取代!=进行判定，结合默认构造迭代器指向SIZE_MAX的特点
  // 方便统一兼容默认构造迭代器==end的概念
  return _index < _table->bucket_count();
}

template <typename T, typename H, typename E>
template <bool CONST>
inline ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::Iterator(
    const ConcurrentFixedSwissTable& table, size_t index) noexcept
    : Iterator(table, index, GroupIterator {}) {}

template <typename T, typename H, typename E>
template <bool CONST>
inline ConcurrentFixedSwissTable<T, H, E>::Iterator<CONST>::Iterator(
    const ConcurrentFixedSwissTable& table, size_t index,
    GroupIterator iter) noexcept
    : _table {const_cast<ConcurrentFixedSwissTable*>(&table)},
      _index {index},
      _iter {iter} {}
// ConcurrentFixedSwissTable::Iterator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentFixedSwissTable begin
template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
ConcurrentFixedSwissTable<T, H, E>::ConcurrentFixedSwissTable() noexcept
    : _controls {Group::s_dummy_controls}, _bucket_mask {Group::GROUP_MASK} {}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
ConcurrentFixedSwissTable<T, H, E>::ConcurrentFixedSwissTable(
    ConcurrentFixedSwissTable&& other) noexcept
    : ConcurrentFixedSwissTable {} {
  *this = ::std::move(other);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
ConcurrentFixedSwissTable<T, H, E>::ConcurrentFixedSwissTable(
    const ConcurrentFixedSwissTable& other) noexcept
    : ConcurrentFixedSwissTable {other.bucket_count()} {
  for (auto& value : other) {
    emplace(value);
  }
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE ConcurrentFixedSwissTable<T, H, E>&
ConcurrentFixedSwissTable<T, H, E>::operator=(
    ConcurrentFixedSwissTable&& other) noexcept {
  swap(other);
  return *this;
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE ConcurrentFixedSwissTable<T, H, E>&
ConcurrentFixedSwissTable<T, H, E>::operator=(
    const ConcurrentFixedSwissTable& other) noexcept {
  clear();
  reserve(other.bucket_count());
  for (auto& value : other) {
    emplace(value);
  }
  return *this;
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
    ConcurrentFixedSwissTable<T, H, E>::~ConcurrentFixedSwissTable() noexcept {
  if (_controls != Group::s_dummy_controls) {
    clear();
    auto allocate_size = calculate_allocate_size(_bucket_mask + 1);
    ::operator delete(_controls, allocate_size, VALUE_ALIGNMENT);
  }
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
ConcurrentFixedSwissTable<T, H, E>::ConcurrentFixedSwissTable(
    size_t min_bucket_count) noexcept {
  construct_with_bucket(min_bucket_count);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE typename ConcurrentFixedSwissTable<T, H, E>::iterator
ConcurrentFixedSwissTable<T, H, E>::begin() noexcept {
  auto base_index_and_iter = find_first_non_empty(0);
  iterator iter {*this, ::std::get<0>(base_index_and_iter),
                 ::std::get<1>(base_index_and_iter)};
  return ++iter;
}

template <typename T, typename H, typename E>
inline typename ConcurrentFixedSwissTable<T, H, E>::const_iterator
ConcurrentFixedSwissTable<T, H, E>::begin() const noexcept {
  auto mutable_this = const_cast<ConcurrentFixedSwissTable*>(this);
  return mutable_this->begin();
}

template <typename T, typename H, typename E>
inline typename ConcurrentFixedSwissTable<T, H, E>::iterator
ConcurrentFixedSwissTable<T, H, E>::end() noexcept {
  return {*this, bucket_count()};
}

template <typename T, typename H, typename E>
inline typename ConcurrentFixedSwissTable<T, H, E>::const_iterator
ConcurrentFixedSwissTable<T, H, E>::end() const noexcept {
  return {*this, bucket_count()};
}

template <typename T, typename H, typename E>
inline bool ConcurrentFixedSwissTable<T, H, E>::empty() const noexcept {
  return size() == 0;
}

template <typename T, typename H, typename E>
inline size_t ConcurrentFixedSwissTable<T, H, E>::size() const noexcept {
  return static_cast<size_t>(_size.value());
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void
ConcurrentFixedSwissTable<T, H, E>::clear() noexcept {
  if (ABSL_PREDICT_FALSE(_controls == Group::s_dummy_controls)) {
    construct_with_bucket(Group::SIZE);
    return;
  }
  if (empty()) {
    return;
  }
  for (size_t i = 0; i < bucket_count(); i += Group::SIZE) {
    auto iter = Group {_controls + i}.match_non_empty();
    if (iter) {
      Group::clear(_controls + i);
      do {
        auto offset = *iter++;
        at(i + offset).~T();
      } while (iter);
    }
  }
  // 补齐SIMD用的镜像位直接清理
  Group::clear(_controls + bucket_count());
  _size.reset();
}

template <typename T, typename H, typename E>
inline ::std::pair<typename ConcurrentFixedSwissTable<T, H, E>::iterator, bool>
ConcurrentFixedSwissTable<T, H, E>::insert(value_type&& value) noexcept {
  return do_emplace(::std::move(value));
}

template <typename T, typename H, typename E>
inline ::std::pair<typename ConcurrentFixedSwissTable<T, H, E>::iterator, bool>
ConcurrentFixedSwissTable<T, H, E>::insert(const value_type& value) noexcept {
  return do_emplace(value);
}

template <typename T, typename H, typename E>
template <typename K, typename... Args, typename>
inline ::std::pair<typename ConcurrentFixedSwissTable<T, H, E>::iterator, bool>
ConcurrentFixedSwissTable<T, H, E>::emplace(K&& key, Args&&... args) noexcept {
  return do_emplace(::std::forward<K>(key), ::std::forward<Args>(args)...);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void ConcurrentFixedSwissTable<T, H, E>::swap(
    ConcurrentFixedSwissTable& other) noexcept {
  ::std::swap(_controls, other._controls);
  ::std::swap(_values, other._values);
  ::std::swap(_bucket_mask, other._bucket_mask);
  ::std::swap(_size, other._size);
}

template <typename T, typename H, typename E>
template <typename K>
inline size_t ConcurrentFixedSwissTable<T, H, E>::count(
    const K& key) const noexcept {
  return contains(key) ? 1 : 0;
}

template <typename T, typename H, typename E>
template <typename K>
ABSL_ATTRIBUTE_NOINLINE typename ConcurrentFixedSwissTable<T, H, E>::iterator
ConcurrentFixedSwissTable<T, H, E>::find(const K& key) noexcept {
  // 哈希值中截取低位用于检测，剩余高位用于桶选择
  auto hash = hasher()(key);
  int8_t checker = hash & Group::CHECKER_MASK;
  auto base_index = (hash >> Group::CHECKER_MASK_BITS) & _bucket_mask;

  // 从起始桶位置开始，以组为单位进行二次探测
  size_t step = 0;
  while (step <= _bucket_mask) {
    // 先通过单独存储的低位哈希值进行粗筛
    Group group {_controls + base_index};
    auto iter = group.match(checker);
    while (iter) {
      auto offset = *iter++;
      auto index = (base_index + offset) & _bucket_mask;
      ::std::atomic_thread_fence(::std::memory_order_acquire);
#if ABSL_HAVE_THREAD_SANITIZER
      __tsan_acquire(_controls + base_index + offset);
#endif // ABSL_HAVE_THREAD_SANITIZER
      // 通过粗筛后使用key比较进行核对
      if (E::extract(at(index)) == key) {
        return {*this, index};
      }
    }
    // 如果当前组存在空洞，则表明无需继续探测
    if (group.match_empty()) {
      break;
    }

    step += Group::SIZE;
    base_index = (base_index + step) & _bucket_mask;
  }

  return end();
}

template <typename T, typename H, typename E>
template <typename K>
inline typename ConcurrentFixedSwissTable<T, H, E>::const_iterator
ConcurrentFixedSwissTable<T, H, E>::find(const K& key) const noexcept {
  auto mutable_this = const_cast<ConcurrentFixedSwissTable*>(this);
  return mutable_this->find(key);
}

template <typename T, typename H, typename E>
template <typename K>
inline bool ConcurrentFixedSwissTable<T, H, E>::contains(
    const K& key) const noexcept {
  return find(key) != end();
}

template <typename T, typename H, typename E>
inline size_t ConcurrentFixedSwissTable<T, H, E>::bucket_count()
    const noexcept {
  return _bucket_mask + 1;
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void ConcurrentFixedSwissTable<T, H, E>::rehash(
    size_t min_bucket_count) noexcept {
  if (ABSL_PREDICT_FALSE(_controls == Group::s_dummy_controls)) {
    construct_with_bucket(min_bucket_count);
    return;
  }

  auto new_bucket_count = ::absl::bit_ceil(min_bucket_count);
  if (new_bucket_count == bucket_count()) {
    return;
  }

  ConcurrentFixedSwissTable saved_table = ::std::move(*this);
  construct_with_bucket(
      ::std::max<size_t>(new_bucket_count, saved_table.size()));
  for (auto& value : saved_table) {
    emplace(::std::move(value));
  }
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void ConcurrentFixedSwissTable<T, H, E>::reserve(
    size_t min_size) noexcept {
  if (ABSL_PREDICT_FALSE(_controls == Group::s_dummy_controls)) {
    construct_with_bucket(min_size);
    return;
  }

  if (min_size > bucket_count()) {
    ConcurrentFixedSwissTable saved_table = ::std::move(*this);
    construct_with_bucket(min_size);
    for (auto& value : saved_table) {
      emplace(::std::move(value));
    }
  }
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void
ConcurrentFixedSwissTable<T, H, E>::construct_with_bucket(
    size_t min_bucket_count) noexcept {
  auto bucket_count =
      ::absl::bit_ceil(::std::max<size_t>(min_bucket_count, Group::SIZE));
  auto allocate_size = calculate_allocate_size(bucket_count);
  auto values_offset = calculate_values_offset(bucket_count);

  auto buffer =
      static_cast<int8_t*>(::operator new(allocate_size, VALUE_ALIGNMENT));
  _controls = reinterpret_cast<::std::atomic<int8_t>*>(buffer);
  _values = reinterpret_cast<CachelineAligned<T>*>(buffer + values_offset);

  __builtin_memset(buffer, Group::EMPTY_CONTROL, values_offset);
  _bucket_mask = bucket_count - 1;
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE size_t
ConcurrentFixedSwissTable<T, H, E>::calculate_allocate_size(
    size_t bucket_count) noexcept {
  auto allocate_size = calculate_values_offset(bucket_count);
  allocate_size += sizeof(CachelineAligned<T>) * bucket_count;
  return align_up(allocate_size, VALUE_ALIGNMENT);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE size_t
ConcurrentFixedSwissTable<T, H, E>::calculate_values_offset(
    size_t bucket_count) noexcept {
  // 补充一些控制字节，保证最后一个有效数据位置往后依然能够读到一个整组
  // 这些组通过循环扩展方式镜像存储头部的控制字节，补齐一个SIMD指令
  auto values_offset = sizeof(int8_t) * (bucket_count + Group::SIZE);
  return align_up(values_offset, VALUE_ALIGNMENT);
}

template <typename T, typename H, typename E>
template <typename K, typename... Args>
ABSL_ATTRIBUTE_NOINLINE ::std::pair<
    typename ConcurrentFixedSwissTable<T, H, E>::iterator, bool>
ConcurrentFixedSwissTable<T, H, E>::do_emplace(K&& key_or_value,
                                               Args&&... args) noexcept {
  // 哈希值中截取低位用于检测，剩余高位用于桶选择
  auto& key = E::extract(key_or_value);
  auto hash = hasher()(key);
  int8_t checker = hash & Group::CHECKER_MASK;
  auto base_index = (hash >> Group::CHECKER_MASK_BITS) & _bucket_mask;

  // 从起始桶位置开始，以组为单位进行二次探测
  size_t step = 0;
  while (step <= _bucket_mask) {
    // 先通过单独存储的低位哈希值进行粗筛
    auto controls = _controls + base_index;
    Group group {controls};
    auto iter = group.match(checker);
    while (iter) {
      auto offset = *iter++;
      auto index = (base_index + offset) & _bucket_mask;
      ::std::atomic_thread_fence(::std::memory_order_acquire);
#if ABSL_HAVE_THREAD_SANITIZER
      __tsan_acquire(_controls + base_index + offset);
#endif // ABSL_HAVE_THREAD_SANITIZER
      // 通过粗筛后使用key比较进行核对
      if (E::extract(at(index)) == key) {
        return {{*this, index}, false};
      }
    }
    // 当前组存在空洞才可以尝试插入，否则继续探测
    iter = group.match_empty();
    if (iter) {
      auto offset = *iter;
      // 为了减少查找分支，对于序号在前Group::SIZE - 1的元素需要
      // 额外向尾部环形追加一份冗余低位哈希
      // 出于统一编码的考虑，所以位置都会写两次低位哈希
      // 只是对于低序号以外的部分冗余序号和原始序号是一致的
      auto index = (base_index + offset) & _bucket_mask;
      auto cloned_index = ((index - Group::GROUP_MASK) & _bucket_mask) +
                          (Group::GROUP_MASK & _bucket_mask);
      auto& control = _controls[index];
      auto& cloned_control = _controls[cloned_index];
      int8_t control_value = Group::EMPTY_CONTROL;
      // 统一采用原始序号竞争写入权限
      if (control.compare_exchange_strong(control_value, Group::BUSY_CONTROL,
                                          ::std::memory_order_acquire,
                                          ::std::memory_order_relaxed)) {
        UsesAllocatorConstructor::construct(&at(index), allocator_type(),
                                            ::std::forward<K>(key_or_value),
                                            ::std::forward<Args>(args)...);
        control.store(checker, ::std::memory_order_release);
        cloned_control.store(checker, ::std::memory_order_release);
        _size << 1;
        return {{*this, index}, true};
      } else if (control_value == Group::DUMMY_CONTROL) {
        // 向默认构造实例插入的特殊情况，直接终止探测
        break;
      } else if (control_value == Group::BUSY_CONTROL) {
        // 其他线程正在插入过程中，避让等待
        ::sched_yield();
        continue;
      } else {
        // 其他线程率先完成了插入，直接返回重试
        continue;
      }
    }

    step += Group::SIZE;
    base_index = (base_index + step) & _bucket_mask;
  }

  return {end(), false};
}

template <typename T, typename H, typename E>
inline T& ConcurrentFixedSwissTable<T, H, E>::at(size_t index) noexcept {
  return _values[index];
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE typename ::std::tuple<
    size_t, typename ConcurrentFixedSwissTable<T, H, E>::GroupIterator>
ConcurrentFixedSwissTable<T, H, E>::find_first_non_empty(
    size_t begin_base_index) const noexcept {
  for (size_t i = begin_base_index; i < bucket_count(); i += Group::SIZE) {
    auto iter = Group {_controls + i}.match_non_empty();
    if (iter) {
      return {i, iter};
    }
  }
  return {bucket_count(), {}};
}
// ConcurrentFixedSwissTable end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientHashSet::Iterator begin
template <typename T, typename H, typename E>
template <bool CONST>
template <bool OTHER_CONST, typename>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentTransientHashSet<T, H, E>::Iterator<CONST>::Iterator(
    const Iterator<OTHER_CONST>& other) noexcept
    : Iterator(other._next, other._iter) {}

template <typename T, typename H, typename E>
template <bool CONST>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
    typename ConcurrentTransientHashSet<T, H,
                                        E>::template Iterator<CONST>::reference
    ConcurrentTransientHashSet<T, H, E>::Iterator<CONST>::operator*()
        const noexcept {
  return *_iter;
}

template <typename T, typename H, typename E>
template <bool CONST>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
    typename ConcurrentTransientHashSet<T, H,
                                        E>::template Iterator<CONST>::pointer
    ConcurrentTransientHashSet<T, H, E>::Iterator<CONST>::operator->()
        const noexcept {
  return &**this;
}

template <typename T, typename H, typename E>
template <bool CONST>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool
ConcurrentTransientHashSet<T, H, E>::Iterator<CONST>::operator==(
    Iterator other) const noexcept {
  return _iter == other._iter;
}

template <typename T, typename H, typename E>
template <bool CONST>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool
ConcurrentTransientHashSet<T, H, E>::Iterator<CONST>::operator!=(
    Iterator other) const noexcept {
  return !(*this == other);
}

template <typename T, typename H, typename E>
template <bool CONST>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
    typename ConcurrentTransientHashSet<T, H, E>::template Iterator<CONST>&
    ConcurrentTransientHashSet<T, H,
                               E>::Iterator<CONST>::operator++() noexcept {
  ++_iter;
  if (ABSL_PREDICT_TRUE(_iter)) {
    return *this;
  }

  auto node = _next;
  while (ABSL_PREDICT_FALSE(node != nullptr)) {
    auto iter = node->table.begin();
    node = node->next.load(::std::memory_order_acquire);
    if (iter) {
      _iter = iter;
      _next = node;
      return *this;
    }
  }

  _iter = TableIterator {};
  _next = nullptr;
  return *this;
}

template <typename T, typename H, typename E>
template <bool CONST>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentTransientHashSet<T, H, E>::Iterator<CONST>::Iterator(
    TableNode* next, TableIterator iter) noexcept
    : _next {next}, _iter {iter} {}
// ConcurrentTransientHashSet::Iterator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientHashSet begin
template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
ConcurrentTransientHashSet<T, H, E>::ConcurrentTransientHashSet(
    ConcurrentTransientHashSet&& other) noexcept
    : ConcurrentTransientHashSet {} {
  *this = ::std::move(other);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
ConcurrentTransientHashSet<T, H, E>::ConcurrentTransientHashSet(
    const ConcurrentTransientHashSet& other) noexcept
    : _head {other.size()} {
  for (auto& value : other) {
    _head.table.emplace(value);
  }
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE ConcurrentTransientHashSet<T, H, E>&
ConcurrentTransientHashSet<T, H, E>::operator=(
    ConcurrentTransientHashSet&& other) noexcept {
  swap(other);
  return *this;
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE ConcurrentTransientHashSet<T, H, E>&
ConcurrentTransientHashSet<T, H, E>::operator=(
    const ConcurrentTransientHashSet& other) noexcept {
  ConcurrentTransientHashSet tmp {other};
  swap(tmp);
  return *this;
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE ConcurrentTransientHashSet<
    T, H, E>::~ConcurrentTransientHashSet() noexcept {
  auto node = _head.next.load(::std::memory_order_relaxed);
  while (node != nullptr) {
    auto next = node->next.load(::std::memory_order_relaxed);
    delete node;
    node = next;
  }
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE
ConcurrentTransientHashSet<T, H, E>::ConcurrentTransientHashSet(
    size_t min_bucket_count) noexcept
    : _head {min_bucket_count} {}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE typename ConcurrentTransientHashSet<T, H, E>::iterator
ConcurrentTransientHashSet<T, H, E>::begin() noexcept {
  auto node = _head.next.load(::std::memory_order_acquire);
  auto iter = _head.table.begin();
  if (iter != _head.table.end()) {
    return {node, iter};
  }
  while (ABSL_PREDICT_FALSE(node != nullptr)) {
    iter = node->table.begin();
    if (iter != node->table.end()) {
      return {nullptr, iter};
    }
    node = _head.next.load(::std::memory_order_acquire);
  }
  return {};
}

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
    typename ConcurrentTransientHashSet<T, H, E>::const_iterator
    ConcurrentTransientHashSet<T, H, E>::begin() const noexcept {
  auto mutable_this = const_cast<ConcurrentTransientHashSet*>(this);
  return mutable_this->begin();
}

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
    typename ConcurrentTransientHashSet<T, H, E>::iterator
    ConcurrentTransientHashSet<T, H, E>::end() noexcept {
  // 由于底层可能由多张表动态增长构成，end没有特意取某个表的end
  // 采用默认迭代器来表达，实际默认迭代器指向SIZE_MAX位置来表达终止
  // 正常的跨表迭代器结束也会设置到这个位置
  return iterator {};
}

template <typename T, typename H, typename E>
inline typename ConcurrentTransientHashSet<T, H, E>::const_iterator
ConcurrentTransientHashSet<T, H, E>::end() const noexcept {
  auto mutable_this = const_cast<ConcurrentTransientHashSet*>(this);
  return mutable_this->end();
}

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool
ConcurrentTransientHashSet<T, H, E>::empty() const noexcept {
  return _head.table.empty();
}

template <typename T, typename H, typename E>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
ConcurrentTransientHashSet<T, H, E>::size() const noexcept {
  auto node = _head.next.load(::std::memory_order_acquire);
  if (ABSL_PREDICT_TRUE(node == nullptr)) {
    return _head.table.size();
  }
  return total_size(node);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void
ConcurrentTransientHashSet<T, H, E>::clear() noexcept {
  auto node = _head.next.load(::std::memory_order_relaxed);
  if (ABSL_PREDICT_TRUE(node == nullptr)) {
    return _head.table.clear();
  }

  *this = ConcurrentTransientHashSet {size()};
}

template <typename T, typename H, typename E>
inline ::std::pair<typename ConcurrentTransientHashSet<T, H, E>::iterator, bool>
ConcurrentTransientHashSet<T, H, E>::insert(value_type&& value) noexcept {
  return emplace(::std::move(value));
}

template <typename T, typename H, typename E>
inline ::std::pair<typename ConcurrentTransientHashSet<T, H, E>::iterator, bool>
ConcurrentTransientHashSet<T, H, E>::insert(const value_type& value) noexcept {
  return emplace(value);
}

template <typename T, typename H, typename E>
template <typename... Args>
inline ::std::pair<typename ConcurrentTransientHashSet<T, H, E>::iterator, bool>
ConcurrentTransientHashSet<T, H, E>::emplace(Args&&... args) noexcept {
  auto node = &_head;
  while (true) {
    auto result = node->table.emplace(::std::forward<Args>(args)...);
    if (result.first != node->table.end()) {
      return {{nullptr, result.first}, result.second};
    }

    auto next = node->next.load(::std::memory_order_acquire);
    if (next == nullptr) {
      auto new_node = new TableNode {node->table.bucket_count() << 1};
      auto success = node->next.compare_exchange_strong(
          next, new_node, ::std::memory_order_acq_rel);
      if (success) {
        next = new_node;
      } else {
        delete new_node;
      }
    }
    node = next;
  }
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void ConcurrentTransientHashSet<T, H, E>::swap(
    ConcurrentTransientHashSet& other) noexcept {
  ::std::swap(_head, other._head);
}

template <typename T, typename H, typename E>
template <typename K>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
ConcurrentTransientHashSet<T, H, E>::count(const K& key) const noexcept {
  return contains(key) ? 1 : 0;
}

template <typename T, typename H, typename E>
template <typename K>
inline typename ConcurrentTransientHashSet<T, H, E>::iterator
ConcurrentTransientHashSet<T, H, E>::find(const K& key) noexcept {
  auto result = _head.table.find(key);
  if (result != _head.table.end()) {
    return {nullptr, result};
  }

  auto node = _head.next.load(::std::memory_order_acquire);
  if (ABSL_PREDICT_FALSE(node != nullptr)) {
    do {
      result = node->table.find(key);
      if (result != node->table.end()) {
        return {nullptr, result};
      }
      node = node->next.load(::std::memory_order_acquire);
    } while (node != nullptr);
  }

  return {};
}

template <typename T, typename H, typename E>
template <typename K>
inline typename ConcurrentTransientHashSet<T, H, E>::const_iterator
ConcurrentTransientHashSet<T, H, E>::find(const K& key) const noexcept {
  auto mutable_this = const_cast<ConcurrentTransientHashSet*>(this);
  return mutable_this->find(key);
}

template <typename T, typename H, typename E>
template <typename K>
inline bool ConcurrentTransientHashSet<T, H, E>::contains(
    const K& key) const noexcept {
  return find(key) != end();
}

template <typename T, typename H, typename E>
inline size_t ConcurrentTransientHashSet<T, H, E>::bucket_count()
    const noexcept {
  return _head.table.bucket_count();
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void ConcurrentTransientHashSet<T, H, E>::rehash(
    size_t min_bucket_count) noexcept {
  auto node = _head.next.load(::std::memory_order_relaxed);
  if (ABSL_PREDICT_TRUE(node == nullptr)) {
    _head.table.rehash(min_bucket_count);
    return;
  }

  min_bucket_count = ::std::max(size(), min_bucket_count);
  ConcurrentTransientHashSet tmp {min_bucket_count};
  for (auto& value : *this) {
    tmp.emplace(::std::move(value));
  }
  *this = ::std::move(tmp);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE void ConcurrentTransientHashSet<T, H, E>::reserve(
    size_t min_size) noexcept {
  auto node = _head.next.load(::std::memory_order_relaxed);
  if (ABSL_PREDICT_TRUE(node == nullptr)) {
    _head.table.reserve(min_size);
    return;
  }

  min_size = ::std::max(size(), min_size);
  ConcurrentTransientHashSet tmp {min_size};
  for (auto& value : *this) {
    tmp.emplace(::std::move(value));
  }
  *this = ::std::move(tmp);
}

template <typename T, typename H, typename E>
ABSL_ATTRIBUTE_NOINLINE size_t ConcurrentTransientHashSet<T, H, E>::total_size(
    TableNode* node) const noexcept {
  auto sum = _head.table.bucket_count();
  while (true) {
    auto next = node->next.load(::std::memory_order_acquire);
    if (next == nullptr) {
      return sum + node->table.size();
    }
    sum += node->table.bucket_count();
    node = next;
  }
}
// ConcurrentTransientHashSet end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientHashMap begin
template <typename K, typename V, typename H>
template <typename KK, typename... Args>
inline ::std::pair<typename ConcurrentTransientHashMap<K, V, H>::iterator, bool>
ConcurrentTransientHashMap<K, V, H>::try_emplace(KK&& key,
                                                 Args&&... args) noexcept {
  return this->emplace(::std::forward<KK>(key), ::std::forward<Args>(args)...);
}

template <typename K, typename V, typename H>
template <typename KK>
inline V& ConcurrentTransientHashMap<K, V, H>::operator[](KK&& key) noexcept {
  return this->emplace(::std::forward<KK>(key)).first->second;
}
// ConcurrentTransientHashMap end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#pragma GCC diagnostic pop
