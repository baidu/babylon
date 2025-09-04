#include "babylon/concurrent/counter.h"

#include "babylon/new.h"

BABYLON_NAMESPACE_BEGIN

// ConcurrentMaxer end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentSummer begin
ConcurrentSummer::Summary ConcurrentSummer::value() const noexcept {
#ifdef __x86_64__
  __m128i summary_value = _mm_setzero_si128();
#else
  int64x2_t summary_value = vmovq_n_s64(0);
#endif
  _storage.for_each([&](const Summary& value) {
  // 等效语句summary += value
  // 但涉及128bit原子写操作，需要特殊处理
#ifdef __x86_64__
    __m128i local_value =
        _mm_load_si128(reinterpret_cast<const __m128i*>(&value));
    summary_value = _mm_add_epi64(summary_value, local_value);
#else
    int64x2_t local_value = vld1q_s64(reinterpret_cast<const int64_t*>(&value));
    summary_value = vaddq_s64(summary_value, local_value);
#endif
  });
  return {summary_value[0], static_cast<size_t>(summary_value[1])};
}
// ConcurrentSummer end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentSampler::Sample begin
ConcurrentSampler::Sample::~Sample() noexcept {
  for (auto bucket : buckets) {
    ::operator delete(bucket, CACHELINE_ALIGNMENT);
  }
}
// ConcurrentSampler::Sample end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentSampler begin
void ConcurrentSampler::set_bucket_capacity(size_t index,
                                            size_t capacity) noexcept {
  _bucket_capacity[index] = capacity;
}

size_t ConcurrentSampler::bucket_capacity(size_t index) const noexcept {
  return _bucket_capacity[index];
}

ConcurrentSampler& ConcurrentSampler::operator<<(uint32_t value) noexcept {
  auto bucket = prepare_sample_bucket(value);
  // record_num只有本计数线程会写入，无需特殊同步
  auto record_num = bucket->record_num.load(::std::memory_order_relaxed);
  // 空间未满直接记入
  if (record_num < bucket->capacity) {
    bucket->data[record_num] = value;
    // 确保汇聚线程读到record_num后样本已经写入
    bucket->record_num.store(record_num + 1, ::std::memory_order_release);
    return *this;
  }
  // 空间满了则按照概率记入
  auto index = xorshift128_rand() % (record_num + 1);
  if (index < bucket->capacity) {
    bucket->data[index] = value;
  }
  bucket->record_num.store(record_num + 1, ::std::memory_order_release);
  return *this;
}

void ConcurrentSampler::reset() noexcept {
  _version.store(_version.load(::std::memory_order_relaxed) + 1,
                 ::std::memory_order_release);
}

ConcurrentSampler::SampleBucket* ConcurrentSampler::prepare_sample_bucket(
    uint32_t value) noexcept {
  auto index = bucket_index(value);
  auto& local = _storage.local();
  // 检测是否开启了新版本
  auto global_version = _version.load(::std::memory_order_acquire);
  if (ABSL_PREDICT_FALSE(local.version.load(::std::memory_order_relaxed) !=
                         global_version)) {
    local.non_empty_bucket_mask.store(0, ::std::memory_order_relaxed);
    local.version.store(global_version, ::std::memory_order_release);
  }
  auto non_empty_bucket_mask =
      local.non_empty_bucket_mask.load(::std::memory_order_relaxed);
  auto bucket = local.buckets[index];
  // 周期内桶首次使用，进行一些清理和调整操作
  if (ABSL_PREDICT_FALSE((non_empty_bucket_mask & (1 << index)) == 0)) {
    // 桶未创建，或者容量需要调整
    if (bucket == nullptr || bucket->capacity != _bucket_capacity[index]) {
      size_t capacity = _bucket_capacity[index];
      size_t allocate_size = sizeof(SampleBucket) + sizeof(uint32_t) * capacity;
      allocate_size = (allocate_size + BABYLON_CACHELINE_SIZE - 1) &
                      static_cast<size_t>(-BABYLON_CACHELINE_SIZE);
      if (bucket == nullptr || allocate_size != bucket->allocate_size) {
        // 周期内首次使用时，因为global_version已经推进，后续reset在看到non_empty_bucket_mask
        // 标记前不会尝试访问桶，可以安全释放
        ::operator delete(bucket, CACHELINE_ALIGNMENT);
        bucket = reinterpret_cast<SampleBucket*>(
            ::operator new(allocate_size, CACHELINE_ALIGNMENT));
        bucket->allocate_size = allocate_size;
        local.buckets[index] = bucket;
      }
      bucket->capacity = capacity;
    }
    bucket->record_num.store(0, ::std::memory_order_relaxed);
    non_empty_bucket_mask |= 1 << index;
    local.non_empty_bucket_mask.store(non_empty_bucket_mask,
                                      ::std::memory_order_release);
  }
  return bucket;
}
// ConcurrentSampler end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
