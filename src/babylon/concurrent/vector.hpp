#pragma once

#include "babylon/concurrent/vector.h"

// clang-format off
#include "babylon/protect.h"
// clang-format on

#include <time.h>

BABYLON_NAMESPACE_BEGIN

namespace internal {
namespace concurrent_vector {
// 基于时间的退役列表容器
// 相比典型的基于epoch锁定的版本，省去了访问侧的额外标记动作
// 而且因为标记被省略，不再依赖thread local等机制来支持快速标记
// 而动态多个thread local存储机制，例如pthread_key，有最大个数等诸多限制
// 但是相对地，基于时间规避竞争为了安全，需要足够长的时间间隔
// 导致淘汰会很不及时，无法支持频繁淘汰的场景
// 不过对于ConcurrentVector中的BlockTable淘汰足够使用
template <typename T, typename D>
class RetireList {
 public:
  // 只能移动不能拷贝
  inline RetireList() noexcept = default;
  inline RetireList(RetireList&& other) noexcept;
  inline RetireList(const RetireList& other) noexcept = delete;
  inline RetireList& operator=(RetireList&& other) noexcept;
  inline RetireList& operator=(const RetireList& other) noexcept = delete;
  inline ~RetireList() noexcept;
  // 退役一个元素，元素所有权会交给容器管理
  // 但暂时不会修改元素内容或释放对象，在后续的退役和回收操作时
  // 会检测已经退役的元素距离当时的时间，如果已经相隔了足够长的时间
  // 在相隔足够长的冷却期之后，会将退役元素确实释放
  void retire(T* data);
  // 检测当前已退役元素是否都距当前有足够的冷却时间
  // 如果可以，将积攒的推移元素进行确实释放
  void gc() noexcept;
  // 不检测时间，直接将积攒的退役元素进行释放
  void unsafe_gc() noexcept;

 private:
  struct Node {
    T* data;
    Node* next;
  };
  // 采用标记指针来压缩存储时间戳和头节点指针，辅助读写标记指针内容
  inline static Node* get_node(uint64_t head) noexcept;
  inline static uint16_t get_timestamp(uint64_t head) noexcept;
  inline static uint64_t make_head(Node* node, uint64_t timestamp) noexcept;
  // 获取和比较uint16_t表示的时间戳，降低精度用于存入标记指针
  inline static uint16_t get_current_timestamp() noexcept;
  inline static bool expire(uint64_t head, uint16_t current_timestamp) noexcept;
  // 删除一条单链表
  static void delete_list(uint64_t head) noexcept;

#if !__clang__ && BABYLON_GCC_VERSION < 50000
  ::std::atomic<uint64_t> _head = ATOMIC_VAR_INIT(0);
#else  // __clang__ || BABYLON_GCC_VERSION >= 50000
  ::std::atomic<uint64_t> _head {0};
#endif // __clang__ || BABYLON_GCC_VERSION >= 50000
};

template <typename T, typename D>
inline RetireList<T, D>::RetireList(RetireList&& other) noexcept
    : _head(other._head.load(::std::memory_order_relaxed)) {
  other._head.store(0, ::std::memory_order_relaxed);
}

template <typename T, typename D>
inline RetireList<T, D>& RetireList<T, D>::operator=(
    RetireList&& other) noexcept {
  auto tmp = _head.load(::std::memory_order_relaxed);
  _head.store(other._head.load(::std::memory_order_relaxed),
              ::std::memory_order_relaxed);
  other._head.store(tmp, ::std::memory_order_relaxed);
  return *this;
}

template <typename T, typename D>
inline RetireList<T, D>::~RetireList() noexcept {
  delete_list(_head.load(::std::memory_order_relaxed));
}

template <typename T, typename D>
void RetireList<T, D>::retire(T* data) {
  auto* node = new Node;
  node->data = data;
  auto head = _head.load(::std::memory_order_acquire);
  auto timestamp = get_current_timestamp();
  auto new_head = make_head(node, timestamp);
  if (expire(head, timestamp)) {
    node->next = nullptr;
    if (_head.compare_exchange_strong(head, new_head,
                                      ::std::memory_order_acq_rel)) {
      delete_list(head);
      return;
    }
  }
  do {
    node->next = get_node(head);
  } while (!_head.compare_exchange_weak(head, new_head,
                                        ::std::memory_order_acq_rel));
}

template <typename T, typename D>
void RetireList<T, D>::gc() noexcept {
  auto head = _head.load(::std::memory_order_acquire);
  auto timestamp = get_current_timestamp();
  if (expire(head, timestamp)) {
    if (_head.compare_exchange_strong(head, 0, ::std::memory_order_acq_rel)) {
      delete_list(head);
    }
  }
}

template <typename T, typename D>
void RetireList<T, D>::unsafe_gc() noexcept {
  auto head = _head.exchange(0, ::std::memory_order_relaxed);
  delete_list(head);
}

template <typename T, typename D>
inline __attribute__((always_inline)) typename RetireList<T, D>::Node*
RetireList<T, D>::get_node(uint64_t head) noexcept {
  return reinterpret_cast<Node*>(head & 0x0000FFFFFFFFFFFFUL);
}

template <typename T, typename D>
inline __attribute__((always_inline)) uint16_t RetireList<T, D>::get_timestamp(
    uint64_t head) noexcept {
  return head >> 48;
}

template <typename T, typename D>
inline __attribute__((always_inline)) uint64_t RetireList<T, D>::make_head(
    Node* node, uint64_t timestamp) noexcept {
  return (timestamp << 48) | reinterpret_cast<uint64_t>(node);
}

template <typename T, typename D>
inline __attribute__((always_inline)) uint16_t
RetireList<T, D>::get_current_timestamp() noexcept {
  ::timespec spec;
  ::clock_gettime(CLOCK_MONOTONIC_RAW, &spec);
  // 按照64s一个时间单位
  return spec.tv_sec >> 6;
}

template <typename T, typename D>
inline __attribute__((always_inline)) bool RetireList<T, D>::expire(
    uint64_t head, uint16_t current_timestamp) noexcept {
  // 超过两个时间单位视为过期，不用一个单位是为了避开时间线附近的跳变
  // 保证判定为过期时至少超过了一个时间单位
  // 理论上由于uint16_t的溢出，会导致2 / 65536比例的漏报
  // 不过漏报并不重要，且比例很低，可忽略
  return static_cast<uint16_t>(current_timestamp - get_timestamp(head)) > 1;
}

template <typename T, typename D>
void RetireList<T, D>::delete_list(uint64_t head) noexcept {
  auto* node = get_node(head);
  while (node != nullptr) {
    auto* next = node->next;
    D()(node->data);
    delete node;
    node = next;
  }
}

} // namespace concurrent_vector
} // namespace internal

////////////////////////////////////////////////////////////////////////////////
// ConcurrentVector::Snapshot begin
template <typename T, size_t BLOCK_SIZE>
inline __attribute__((always_inline)) T&
ConcurrentVector<T, BLOCK_SIZE>::Snapshot::operator[](
    size_t index) const noexcept {
  return _block_table
      ->blocks[_meta.block_index(index)][_meta.block_offset(index)];
}

template <typename T, size_t BLOCK_SIZE>
template <typename IT, typename>
inline void ConcurrentVector<T, BLOCK_SIZE>::Snapshot::copy_n(IT begin,
                                                              size_t size,
                                                              size_t offset) {
  for_each(offset, offset + size, [&](T* iter, T* end) {
    auto num = end - iter;
    ::std::copy_n(begin, num, iter);
    begin += num;
  });
}

template <typename T, size_t BLOCK_SIZE>
inline void ConcurrentVector<T, BLOCK_SIZE>::Snapshot::fill_n(size_t offset,
                                                              size_t size,
                                                              const T& value) {
  for_each(offset, offset + size, [&](T* iter, T* end) {
    ::std::fill(iter, end, value);
  });
}

template <typename T, size_t BLOCK_SIZE>
template <typename C, typename>
inline void ConcurrentVector<T, BLOCK_SIZE>::Snapshot::for_each(size_t begin,
                                                                size_t end,
                                                                C&& callback) {
  auto block_index = _meta.block_index(begin);
  auto block_offset = _meta.block_offset(begin);
  auto end_block_index = _meta.block_index(end);
  auto end_block_offset = _meta.block_offset(end);
  while (block_index != end_block_index) {
    T* block_begin = _block_table->blocks[block_index];
    callback(block_begin + block_offset, block_begin + _meta.block_size());
    block_index += 1;
    block_offset = 0;
  }
  if (block_offset != end_block_offset) {
    T* block_begin = _block_table->blocks[block_index];
    callback(block_begin + block_offset, block_begin + end_block_offset);
  }
}

template <typename T, size_t BLOCK_SIZE>
template <typename C, typename>
inline void ConcurrentVector<T, BLOCK_SIZE>::Snapshot::for_each(
    size_t begin, size_t end, C&& callback) const {
  auto block_index = _meta.block_index(begin);
  auto block_offset = _meta.block_offset(begin);
  auto end_block_index = _meta.block_index(end);
  auto end_block_offset = _meta.block_offset(end);
  while (block_index != end_block_index) {
    const T* block_begin = _block_table->blocks[block_index];
    callback(block_begin + block_offset, block_begin + _meta.block_size());
    block_index += 1;
    block_offset = 0;
  }
  if (block_offset != end_block_offset) {
    const T* block_begin = _block_table->blocks[block_index];
    callback(block_begin + block_offset, block_begin + end_block_offset);
  }
}

template <typename T, size_t BLOCK_SIZE>
inline size_t ConcurrentVector<T, BLOCK_SIZE>::Snapshot::size() const noexcept {
  return _block_table->size << _meta.block_mask_bits();
}

template <typename T, size_t BLOCK_SIZE>
inline ConcurrentVector<T, BLOCK_SIZE>::Snapshot::Snapshot(
    Meta meta, BlockTable* block_table) noexcept
    : _meta(meta), _block_table(block_table) {}
// ConcurrentVector::Snapshot end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentVector::ConstSnapshot begin
template <typename T, size_t BLOCK_SIZE>
inline const T& ConcurrentVector<T, BLOCK_SIZE>::ConstSnapshot::operator[](
    size_t index) const noexcept {
  return _snapshot[index];
}

template <typename T, size_t BLOCK_SIZE>
template <typename C, typename>
inline void ConcurrentVector<T, BLOCK_SIZE>::ConstSnapshot::for_each(
    size_t begin, size_t end, C&& callback) const {
  _snapshot.for_each(begin, end, ::std::forward<C>(callback));
}

template <typename T, size_t BLOCK_SIZE>
inline size_t ConcurrentVector<T, BLOCK_SIZE>::ConstSnapshot::size()
    const noexcept {
  return _snapshot.size();
}

template <typename T, size_t BLOCK_SIZE>
inline ConcurrentVector<T, BLOCK_SIZE>::ConstSnapshot::ConstSnapshot(
    Meta meta, BlockTable* block_table) noexcept
    : _snapshot(meta, block_table) {}
// ConcurrentVector::ConstSnapshot end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentVector::BlockTableDeleter begin
template <typename T, size_t BLOCK_SIZE>
inline __attribute__((always_inline)) void
ConcurrentVector<T, BLOCK_SIZE>::BlockTableDeleter::operator()(
    BlockTable* table) noexcept {
  delete_block_table(table);
}
// ConcurrentVector::BlockTableDeleter end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentVector::DynamicMeta begin
template <typename T, size_t BLOCK_SIZE>
inline size_t ConcurrentVector<T, BLOCK_SIZE>::DynamicMeta::set_block_size(
    size_t block_size_hint) noexcept {
  uint32_t block_size = 1;
  _block_mask = 0;
  _block_mask_bits = 0;
  while (block_size < block_size_hint) {
    ++_block_mask_bits;
    _block_mask <<= 1;
    _block_mask |= 1;
    block_size <<= 1;
  }
  return block_size;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::DynamicMeta::block_size()
    const noexcept {
  return _block_mask + 1;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::DynamicMeta::block_mask()
    const noexcept {
  return _block_mask;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::DynamicMeta::block_mask_bits()
    const noexcept {
  return _block_mask_bits;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::DynamicMeta::block_index(
    size_t index) const noexcept {
  return index >> _block_mask_bits;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::DynamicMeta::block_offset(
    size_t index) const noexcept {
  return index & _block_mask;
}
// ConcurrentVector::DynamicMeta end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentVector::StaticMeta begin
template <typename T, size_t BLOCK_SIZE>
inline size_t ConcurrentVector<T, BLOCK_SIZE>::StaticMeta::set_block_size(
    size_t) noexcept {
  return BLOCK_SIZE;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::StaticMeta::block_size()
    const noexcept {
  return BLOCK_SIZE;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::StaticMeta::block_mask()
    const noexcept {
  return BLOCK_SIZE - 1;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::StaticMeta::block_mask_bits()
    const noexcept {
  return BLOCK_MASK_BITS;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::StaticMeta::block_index(
    size_t index) const noexcept {
  return index >> BLOCK_MASK_BITS;
}

template <typename T, size_t BLOCK_SIZE>
inline uint32_t ConcurrentVector<T, BLOCK_SIZE>::StaticMeta::block_offset(
    size_t index) const noexcept {
  return index & (BLOCK_SIZE - 1);
}
// ConcurrentVector::StaticMeta end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentVector begin
template <typename T, size_t BLOCK_SIZE>
inline ConcurrentVector<T, BLOCK_SIZE>::ConcurrentVector() noexcept
    : ConcurrentVector {DEFAULT_BLOCK_SIZE} {}

template <typename T, size_t BLOCK_SIZE>
inline ConcurrentVector<T, BLOCK_SIZE>::ConcurrentVector(
    ConcurrentVector&& other) noexcept
    : ConcurrentVector {other._constructor} {
  swap(other);
}

template <typename T, size_t BLOCK_SIZE>
inline ConcurrentVector<T, BLOCK_SIZE>&
ConcurrentVector<T, BLOCK_SIZE>::operator=(ConcurrentVector&& other) noexcept {
  swap(other);
  return *this;
}

template <typename T, size_t BLOCK_SIZE>
inline ConcurrentVector<T, BLOCK_SIZE>::~ConcurrentVector() noexcept {
  auto block_table = _block_table.load(::std::memory_order_relaxed);
  for (size_t i = 0; i < block_table->size; ++i) {
    delete_block(block_table->blocks[i]);
  }
  delete_block_table(block_table);
  _retire_list.unsafe_gc();
}

template <typename T, size_t BLOCK_SIZE>
inline ConcurrentVector<T, BLOCK_SIZE>::ConcurrentVector(
    size_t block_size_hint) noexcept
    : ConcurrentVector {block_size_hint, ::std::is_trivial<T>::value
                                             ? ::std::function<void(T*)> {}
                                             : default_constructor<T>} {}

template <typename T, size_t BLOCK_SIZE>
template <typename C, typename>
inline ConcurrentVector<T, BLOCK_SIZE>::ConcurrentVector(
    C&& constructor) noexcept
    : ConcurrentVector {DEFAULT_BLOCK_SIZE, ::std::forward<C>(constructor)} {}

template <typename T, size_t BLOCK_SIZE>
template <typename C, typename>
inline ConcurrentVector<T, BLOCK_SIZE>::ConcurrentVector(
    size_t block_size_hint, C&& constructor) noexcept
    : _constructor {::std::forward<C>(constructor)} {
  _meta.set_block_size(block_size_hint);
  _meta.set_block_size(block_size_hint);
  _block_table.store(const_cast<BlockTable*>(&EMPTY_BLOCK_TABLE),
                     ::std::memory_order_relaxed);
}

template <typename T, size_t BLOCK_SIZE>
void ConcurrentVector<T, BLOCK_SIZE>::set_constructor(
    ::std::function<void(T*)> constructor) noexcept {
  _constructor = constructor;
  if (!_constructor) {
    return;
  }
  for_each(0, size(), [&](T* iter, T* end) {
    while (iter != end) {
      iter->~T();
      _constructor(iter);
      ++iter;
    }
  });
}

template <typename T, size_t BLOCK_SIZE>
inline void ConcurrentVector<T, BLOCK_SIZE>::reserve(size_t size) {
  get_qualified_block_table(_meta.block_index(size + _meta.block_mask()));
}

template <typename T, size_t BLOCK_SIZE>
inline T& ConcurrentVector<T, BLOCK_SIZE>::ensure(size_t index) {
  auto block_index = _meta.block_index(index);
  auto* block_table = get_qualified_block_table(block_index + 1);
  return block_table->blocks[block_index][_meta.block_offset(index)];
}

template <typename T, size_t BLOCK_SIZE>
inline size_t ConcurrentVector<T, BLOCK_SIZE>::size() const noexcept {
  return snapshot().size();
}

template <typename T, size_t BLOCK_SIZE>
inline size_t ConcurrentVector<T, BLOCK_SIZE>::block_size() const noexcept {
  return _meta.block_size();
}

template <typename T, size_t BLOCK_SIZE>
inline T& ConcurrentVector<T, BLOCK_SIZE>::operator[](size_t index) noexcept {
  return snapshot()[index];
}

template <typename T, size_t BLOCK_SIZE>
inline const T& ConcurrentVector<T, BLOCK_SIZE>::operator[](
    size_t index) const noexcept {
  return snapshot()[index];
}

template <typename T, size_t BLOCK_SIZE>
inline void ConcurrentVector<T, BLOCK_SIZE>::fill_n(size_t offset, size_t size,
                                                    const T& value) {
  reserved_snapshot(offset + size).fill_n(offset, size, value);
}

template <typename T, size_t BLOCK_SIZE>
template <typename IT, typename>
inline void ConcurrentVector<T, BLOCK_SIZE>::copy_n(IT begin, size_t size,
                                                    size_t offset) {
  reserved_snapshot(offset + size).copy_n(begin, size, offset);
}

template <typename T, size_t BLOCK_SIZE>
template <typename C, typename>
inline void ConcurrentVector<T, BLOCK_SIZE>::for_each(size_t begin, size_t end,
                                                      C&& callback) {
  reserved_snapshot(end).for_each(begin, end, ::std::forward<C>(callback));
}

template <typename T, size_t BLOCK_SIZE>
template <typename C, typename>
inline void ConcurrentVector<T, BLOCK_SIZE>::for_each(size_t begin, size_t end,
                                                      C&& callback) const {
  snapshot().for_each(begin, end, ::std::forward<C>(callback));
}

template <typename T, size_t BLOCK_SIZE>
inline typename ConcurrentVector<T, BLOCK_SIZE>::Snapshot
ConcurrentVector<T, BLOCK_SIZE>::snapshot() noexcept {
  return Snapshot(_meta, _block_table.load(::std::memory_order_acquire));
}

template <typename T, size_t BLOCK_SIZE>
inline typename ConcurrentVector<T, BLOCK_SIZE>::ConstSnapshot
ConcurrentVector<T, BLOCK_SIZE>::snapshot() const noexcept {
  return ConstSnapshot(_meta, _block_table.load(::std::memory_order_acquire));
}

template <typename T, size_t BLOCK_SIZE>
inline typename ConcurrentVector<T, BLOCK_SIZE>::Snapshot
ConcurrentVector<T, BLOCK_SIZE>::reserved_snapshot(size_t size) {
  auto* block_table =
      get_qualified_block_table(_meta.block_index(size + _meta.block_mask()));
  return Snapshot(_meta, block_table);
}

template <typename T, size_t BLOCK_SIZE>
inline void ConcurrentVector<T, BLOCK_SIZE>::gc() {
  _retire_list.gc();
}

template <typename T, size_t BLOCK_SIZE>
inline void ConcurrentVector<T, BLOCK_SIZE>::unsafe_gc() {
  _retire_list.unsafe_gc();
}

template <typename T, size_t BLOCK_SIZE>
void ConcurrentVector<T, BLOCK_SIZE>::swap(ConcurrentVector& other) noexcept {
  {
    auto tmp = _meta;
    _meta = other._meta;
    other._meta = tmp;
  }
  ::std::swap(_constructor, other._constructor);
  {
    auto tmp = _block_table.load(::std::memory_order_relaxed);
    _block_table.store(other._block_table.load(::std::memory_order_relaxed),
                       ::std::memory_order_relaxed);
    other._block_table.store(tmp, ::std::memory_order_relaxed);
  }
  ::std::swap(_retire_list, other._retire_list);
}

template <typename T, size_t BLOCK_SIZE>
template <typename U>
void ConcurrentVector<T, BLOCK_SIZE>::default_constructor(U* ptr) noexcept {
  new (ptr) U;
}

template <typename T, size_t BLOCK_SIZE>
inline typename ConcurrentVector<T, BLOCK_SIZE>::BlockTable*
ConcurrentVector<T, BLOCK_SIZE>::create_block_table(size_t num) noexcept {
  auto allocation_size = calculate_block_table_allocation_size(num);
  auto* block_table = reinterpret_cast<BlockTable*>(::operator new(
      allocation_size, ::std::align_val_t(BABYLON_CACHELINE_SIZE)));
  block_table->size = num;
  return block_table;
}

template <typename T, size_t BLOCK_SIZE>
inline void ConcurrentVector<T, BLOCK_SIZE>::delete_block_table(
    BlockTable* block_table) noexcept {
  if (block_table != &EMPTY_BLOCK_TABLE) {
    auto allocation_size =
        calculate_block_table_allocation_size(block_table->size);
    ::operator delete(block_table, allocation_size,
                      ::std::align_val_t(BABYLON_CACHELINE_SIZE));
  }
}

template <typename T, size_t BLOCK_SIZE>
inline size_t
ConcurrentVector<T, BLOCK_SIZE>::calculate_block_table_allocation_size(
    size_t num) noexcept {
  auto allocation_size = sizeof(BlockTable) + sizeof(char*) * num;
  return align_up(allocation_size, ::std::align_val_t(BABYLON_CACHELINE_SIZE));
}

template <typename T, size_t BLOCK_SIZE>
inline T* ConcurrentVector<T, BLOCK_SIZE>::create_block() {
  auto allocation_size = calculate_block_allocation_size();
  auto* block =
      reinterpret_cast<T*>(::operator new(allocation_size, block_alignment()));
  if (_constructor) {
    for (size_t i = 0; i < _meta.block_size(); ++i) {
      _constructor(block + i);
    }
  } else {
    __builtin_memset(reinterpret_cast<void*>(block), 0, allocation_size);
  }
  return block;
}

template <typename T, size_t BLOCK_SIZE>
inline size_t
ConcurrentVector<T, BLOCK_SIZE>::calculate_block_allocation_size() noexcept {
  auto allocation_size = sizeof(T) * _meta.block_size();
  return align_up(allocation_size, block_alignment());
}

template <typename T, size_t BLOCK_SIZE>
inline constexpr ::std::align_val_t
ConcurrentVector<T, BLOCK_SIZE>::block_alignment() noexcept {
  return ::std::align_val_t(
      ::std::max<size_t>(BABYLON_CACHELINE_SIZE, alignof(T)));
}

template <typename T, size_t BLOCK_SIZE>
inline void ConcurrentVector<T, BLOCK_SIZE>::delete_block(T* block) noexcept {
  if (!::std::is_trivial<T>::value) {
    for (size_t i = 0; i < _meta.block_size(); ++i) {
      block[i].~T();
    }
  }
  auto alignment = ::std::max<size_t>(BABYLON_CACHELINE_SIZE, alignof(T));
  auto allocation_size = calculate_block_allocation_size();
  ::operator delete(block, allocation_size, ::std::align_val_t(alignment));
}

template <typename T, size_t BLOCK_SIZE>
inline typename ConcurrentVector<T, BLOCK_SIZE>::BlockTable*
ConcurrentVector<T, BLOCK_SIZE>::get_qualified_block_table(
    size_t expect_block_num) {
  auto* block_table = _block_table.load(::std::memory_order_acquire);
  if (block_table->size >= expect_block_num) {
    return block_table;
  }
  return get_qualified_block_table_slow(block_table, expect_block_num);
}

template <typename T, size_t BLOCK_SIZE>
__attribute__((noinline)) typename ConcurrentVector<T, BLOCK_SIZE>::BlockTable*
ConcurrentVector<T, BLOCK_SIZE>::get_qualified_block_table_slow(
    BlockTable* block_table, size_t expect_block_num) {
  auto block_num = block_table->size;
  auto* new_block_table = create_block_table(expect_block_num);
  while (true) {
    __builtin_memcpy(new_block_table->blocks, block_table->blocks,
                     block_num * sizeof(char*));
    for (auto i = block_num; i < expect_block_num; ++i) {
      new_block_table->blocks[i] = create_block();
    }
    if (_block_table.compare_exchange_strong(block_table, new_block_table,
                                             ::std::memory_order_acq_rel,
                                             ::std::memory_order_acquire)) {
      _retire_list.retire(block_table);
      return new_block_table;
    }
    for (auto i = block_num; i < expect_block_num; ++i) {
      delete_block(new_block_table->blocks[i]);
    }
    block_num = block_table->size;
    if (block_num >= expect_block_num) {
      delete_block_table(new_block_table);
      return block_table;
    }
  }
}

#if __cplusplus < 201703L
template <typename T, size_t BLOCK_SIZE>
constexpr typename ConcurrentVector<T, BLOCK_SIZE>::BlockTable
    ConcurrentVector<T, BLOCK_SIZE>::EMPTY_BLOCK_TABLE;
#endif // __cplusplus < 201703L
// ConcurrentVector end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

// clang-format off
#include "babylon/unprotect.h"
// clang-format on
