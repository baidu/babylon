#pragma once

#include "babylon/concurrent/thread_local.h" // CompactEnumerableThreadLocal
#include "babylon/environment.h"

#include <type_traits> // std::is_integral, std::is_floating_point

#ifdef __x86_64__
#include <x86intrin.h>
#endif // __x86_64__

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

BABYLON_NAMESPACE_BEGIN

// 高并发累加计数器
// 原理上等价于利用std::atomic
// 计数操作进行fetch_add(value)
// 读取操作进行load(value)
//
// 实现上针对多写少读的场景做了优化，更适用于典型的计数场景
// 计数操作改为独立发生在对应的线程局部数据上，避免了缓存行竞争
// 但相应的读操作就需要遍历所有线程局部数据并再次累加才能得到结果
template <typename T>
class GenericsConcurrentAdder {
  static_assert((::std::is_integral<T>::value ||
                 ::std::is_floating_point<T>::value) &&
                    8 >= sizeof(T),
                "GenericsConcurrentAdder only supports integral or "
                "floating point types with size <= 8 bytes");

 public:
  GenericsConcurrentAdder() noexcept = default;
  GenericsConcurrentAdder(GenericsConcurrentAdder&&) noexcept = default;
  GenericsConcurrentAdder(const GenericsConcurrentAdder&) = delete;
  GenericsConcurrentAdder& operator=(GenericsConcurrentAdder&&) noexcept =
      default;
  GenericsConcurrentAdder& operator=(const GenericsConcurrentAdder&) = delete;
  ~GenericsConcurrentAdder() noexcept = default;

  // 分散计数接口
  template <typename U>
  GenericsConcurrentAdder& operator<<(const U& value) noexcept {
    count(static_cast<T>(value));
    return *this;
  }

  // 汇聚读取接口
  T value() const noexcept {
    T sum = 0;
    _storage.for_each([&](const T& value) {
      sum += value;
    });
    return sum;
  }

  // 重置接口
  void reset() noexcept {
    _storage.for_each([&](T& value) {
      value = 0;
    });
  }

 private:
  void count(T value) noexcept {
    auto& local = _storage.local();
    // local的唯一修改者是自己，所以这里不需要使用原子加法
    // 正确对齐的数值类型赋值本身是原子发生的
    local = local + value;
  }

  CompactEnumerableThreadLocal<T, 64, true> _storage;
};

using ConcurrentAdder = GenericsConcurrentAdder<ssize_t>;

// 高并发最大/小值计数器
// 原理上等价于利用std::atomic
// 计数操作进行loop cas(old, max(new, old)) / cas(old, min(new, old))
// 读取&重置操作进行exchange(0)
//
// 实现上针对多写少读的场景做了优化，更适用于典型的计数场景
// 计数操作改为独立发生在对应的线程局部数据上，避免了缓存行竞争
// 但相应的读操作就需要遍历所有线程局部数据并再次累加才能得到结果
namespace internal {
template <typename T>
struct MaxComparer {
  bool operator()(T lhs, T rhs) const {
    return lhs > rhs;
  }
};
template <typename T>
struct MinComparer {
  bool operator()(T lhs, T rhs) const {
    return lhs < rhs;
  }
};

template <typename T, bool Max>
class ConcurrentComparer {
  static_assert((::std::is_integral<T>::value ||
                 ::std::is_floating_point<T>::value) &&
                    8 >= sizeof(T),
                "ConcurrentComparer only supports integral or "
                "floating point types with size <= 8 bytes");

  constexpr static T EXTREMUM =
      Max ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
  typedef typename std::conditional<Max, MaxComparer<T>, MinComparer<T>>::type
      Comparer;

 public:
  ConcurrentComparer() noexcept = default;
  ConcurrentComparer(ConcurrentComparer&&) = delete;
  ConcurrentComparer(const ConcurrentComparer&) = delete;
  ConcurrentComparer& operator=(ConcurrentComparer&&) = delete;
  ConcurrentComparer& operator=(const ConcurrentComparer&) = delete;
  ~ConcurrentComparer() noexcept = default;

  // 分散计数接口
  ConcurrentComparer& operator<<(T value) noexcept {
    auto& local = _storage.local();
    // 为了避免原子操作引入内存屏障，这里采用异步版本感知机制
    // 取代了cas动作来实现重置效果
    //
    // 理论上存在一个缺陷，即
    // 1、刚刚完成一次汇聚读取，正在进行版本推进
    // 2、此时进行中的计数动作依然计入了上一个版本
    // 3、下一个汇聚读取周期中，这些样本无法计入
    // 不过对于统计场景，这个影响非常微弱，可忽略不计
    if (ABSL_PREDICT_FALSE(_version != local.version)) {
      local.version = _version;
      local.value = value;
      return *this;
    }
    if (ABSL_PREDICT_FALSE(_comparer(value, local.value))) {
      local.value = value;
    }
    return *this;
  }

  // 汇聚读取接口
  // 如果周期内有计数发生，返回计数最大值
  // 如果周期内没有计数发生，返回0
  T value() const noexcept {
    T compare_value = 0;
    value(compare_value);
    return compare_value;
  }

  // 汇聚读取接口
  // 如果周期内有计数发生，返回true，并将compare_value填充为计数最大值
  // 如果周期内没有计数发生，返回false，此时不修改compare_value的值
  bool value(T& compare_value) const noexcept {
    bool has_result = false;
    T result = EXTREMUM;
    _storage.for_each([&](const Slot& slot) {
      if (slot.version == _version) {
        if (_comparer(slot.value, result)) {
          result = slot.value;
          has_result = true;
        }
      }
    });
    if (has_result) {
      compare_value = result;
    }
    return has_result;
  }

  // 重置接口，开启一个新的周期
  void reset() noexcept {
    ++_version;
  }

 private:
  struct Slot {
    size_t version {SIZE_MAX};
    T value;
  };

  CompactEnumerableThreadLocal<Slot, 64, true> _storage;
  size_t _version {0};
  Comparer _comparer;
};
} // namespace internal

template <typename T>
class GenericsConcurrentMaxer : public internal::ConcurrentComparer<T, true> {};

using ConcurrentMaxer = GenericsConcurrentMaxer<ssize_t>;

template <typename T>
class GenericsConcurrentMiner : public internal::ConcurrentComparer<T, false> {
};

using ConcurrentMiner = GenericsConcurrentMiner<ssize_t>;

// 高并发求和计数器
// 原理上等价于利用锁同步
// 计数操作进行lock {sum += value; num += 1}
// 读取操作进行lock {sum; num}
//
// 实现上针对多写少读的场景做了优化，更适用于典型的计数场景
// 计数操作改为独立发生在对应的线程局部数据上，避免了缓存行竞争
class ConcurrentSummer {
 public:
  // 计数结果二元组，总和&总量
  struct Summary {
    ssize_t sum;
    size_t num;
  };

  ConcurrentSummer() noexcept = default;
  ConcurrentSummer(ConcurrentSummer&&) = delete;
  ConcurrentSummer(const ConcurrentSummer&) = delete;
  ConcurrentSummer& operator=(ConcurrentSummer&&) = delete;
  ConcurrentSummer& operator=(const ConcurrentSummer&) = delete;
  ~ConcurrentSummer() noexcept = default;

  // 分散计数接口
  // sum += value; num += 1;
  inline ConcurrentSummer& operator<<(ssize_t value) noexcept;

  // 分散计数接口
  // sum += summary.sum; num += summary.num;
  inline ConcurrentSummer& operator<<(Summary summary) noexcept;

  // 汇聚读取接口
  Summary value() const noexcept;

 private:
  CompactEnumerableThreadLocal<Summary, 64, true> _storage;
};

// 高并发采样计数器
// 原理上等价于利用锁同步
// 计数操作进行lock {if (命中蓄水池采样) sample << value}
// 读取操作进行lock {sample}
//
// 为了节省空间sample本身采用不同值域分桶方式记录
// value [0, 2) => bucket 0
// value [2, 2^31) => bucket log2(n)
// value [2^31, 2^32) => bucket 30
//
// 实现上针对多写少读的场景做了优化，更适用于典型的计数场景
// 计数操作改为独立发生在对应的线程局部数据上，避免了缓存行竞争
class ConcurrentSampler {
 public:
  struct SampleBucket;

  // 可默认构造，不能移动和拷贝
  ConcurrentSampler() noexcept = default;
  ConcurrentSampler(ConcurrentSampler&&) = delete;
  ConcurrentSampler(const ConcurrentSampler&) = delete;
  ConcurrentSampler& operator=(ConcurrentSampler&&) = delete;
  ConcurrentSampler& operator=(const ConcurrentSampler&) = delete;
  ~ConcurrentSampler() noexcept = default;

  // 计算目标值所在的分桶
  inline static size_t bucket_index(uint32_t value) noexcept;

  // 设置目标分桶的预期容量，支持运行中动态修改
  // 修改会在下一轮reset之后生效
  void set_bucket_capacity(size_t index, size_t capacity) noexcept;
  size_t bucket_capacity(size_t index) const noexcept;

  // 计数操作
  ConcurrentSampler& operator<<(uint32_t value) noexcept;

  // 遍历收集各个线程的采样桶，并调用
  // C(size_t bucket_index, const SampleBucket&)
  template <typename C>
  void for_each(C&& callback) const noexcept;

  // 逻辑清理当前积累的样本
  void reset() noexcept;

 public:
  // 一个数值区段的采样桶
  struct SampleBucket {
    // 桶分配大小，用户无需关心
    uint16_t allocate_size;
    // 桶容量，采样总数最多不会超过此值
    uint16_t capacity;
    // 采样前数据总量，超出capacity之后只保留capacity
    ::std::atomic<uint32_t> record_num;
    // 实际采样值
    uint32_t data[0];
  };

 private:
  struct Sample {
    ~Sample() noexcept;

    ::std::atomic<uint32_t> version {0};
    ::std::atomic<uint32_t> non_empty_bucket_mask {0};
    SampleBucket* buckets[31] = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };
  };

  inline static uint16_t xorshift128_rand() noexcept;

  SampleBucket* prepare_sample_bucket(uint32_t value) noexcept;

  EnumerableThreadLocal<Sample, true> _storage;
  uint8_t _bucket_capacity[32] = {30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
                                  30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
                                  30, 30, 30, 30, 30, 30, 30, 30, 30, 30};
  ::std::atomic<uint32_t> _version {0};
};

inline ABSL_ATTRIBUTE_ALWAYS_INLINE ConcurrentSummer&
ConcurrentSummer::operator<<(ssize_t value) noexcept {
  return operator<<({value, 1});
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE ConcurrentSummer&
ConcurrentSummer::operator<<(Summary summary) noexcept {
  auto& local = _storage.local();
  // 等效语句local += summary
  // 但涉及128bit原子写操作，需要特殊处理
#ifdef __x86_64__
  // 对于128bit写操作，Intel和AMD官方声明中均承诺未原子性
  // 但是参考https://rigtorp.se/isatomic/
  // 当前主流X86服务端CPU，单cacheline内128bit对齐的load/store其实是原子的
  // 这里主动使用sse指令即可保证原子性写入
  __m128i delta_value = _mm_load_si128(reinterpret_cast<__m128i*>(&summary));
  __m128i local_value = _mm_load_si128(reinterpret_cast<__m128i*>(&local));
  local_value = _mm_add_epi64(local_value, delta_value);
  _mm_store_si128(reinterpret_cast<__m128i*>(&local), local_value);
#else
  // Armv8.4-A开始，主流服务端CPU均支持对齐场景下128bit的读写原子性
  // 这里主动使用neon生成128bit写操作
  int64x2_t delta_value = vld1q_s64(reinterpret_cast<const int64_t*>(&summary));
  int64x2_t local_value = vld1q_s64(reinterpret_cast<const int64_t*>(&local));
  local_value = vaddq_s64(local_value, delta_value);
  vst1q_s64(reinterpret_cast<int64_t*>(&local), local_value);
#endif
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
// ConcurrentSampler begin
inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
ConcurrentSampler::bucket_index(uint32_t value) noexcept {
  // [0, 2) => 0
  // [2, 2^31) => log2(n)
  // [2^31, 2^32) => 30
  value &= 0x7FFFFFFF;
  value >>= 1;
  if (ABSL_PREDICT_FALSE(value == 0)) {
    return 0;
  } else {
    return 32 - static_cast<size_t>(__builtin_clz(value));
  }
}

template <typename C>
void ConcurrentSampler::for_each(C&& callback) const noexcept {
  auto version = _version.load(::std::memory_order_relaxed);
  _storage.for_each([&](const Sample* iter, const Sample* end) {
    for (; iter != end; ++iter) {
      uint32_t local_version = iter->version.load(::std::memory_order_acquire);
      // 跳过版本不匹配的线程，这些线程在本周期内未计数
      if (local_version != version) {
        continue;
      }
      // 根据mask找到有值的采样桶
      uint32_t mask =
          iter->non_empty_bucket_mask.load(::std::memory_order_acquire);
      while (mask != 0) {
        auto index = __builtin_ctzl(mask);
        mask &= mask - 1;
        auto bucket = iter->buckets[index];
        callback(index, *bucket);
      }
    }
  });
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE uint16_t
ConcurrentSampler::xorshift128_rand() noexcept {
  thread_local uint64_t seed[] {1, 1};
  thread_local uint64_t value {0};
  if (value == 0) {
    uint64_t s1 = seed[0];
    uint64_t s0 = seed[1];
    s1 ^= s1 << 23;
    s1 = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    seed[0] = s0;
    seed[1] = s1;
    value = s1 + s0;
  }
  uint16_t result = value;
  value >>= 16;
  return result;
}
// ConcurrentSampler end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
