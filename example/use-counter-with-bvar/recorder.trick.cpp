#include "recorder.h"

BABYLON_NAMESPACE_BEGIN

void BvarPercentile::merge_into_global_samples(
    size_t index, const ::babylon::ConcurrentSampler::SampleBucket& bucket,
    ::bvar::detail::GlobalPercentileSamples& global_samples) {
  static constexpr size_t SAMPLE_SIZE =
      ::bvar::detail::GlobalPercentileSamples::SAMPLE_SIZE;

  auto& interval = global_samples.get_interval_at(index);
  auto num_added = bucket.record_num.load(::std::memory_order_acquire);
  if (num_added == 0) {
    return;
  }
  auto num_samples =
      ::std::min(num_added, static_cast<uint32_t>(bucket.capacity));
  //  有空间直接存入
  if (interval._num_samples + num_samples <= SAMPLE_SIZE) {
    __builtin_memcpy(interval._samples + interval._num_samples, bucket.data,
                     sizeof(uint32_t) * num_samples);
    interval._num_samples += num_samples;
  } else {
    // 样本概率加权
    float ratio = static_cast<float>(num_samples) / num_added;
    // 先尽量直接存入
    if (interval._num_samples < SAMPLE_SIZE) {
      auto copy_size = SAMPLE_SIZE - interval._num_samples;
      num_samples -= copy_size;
      __builtin_memcpy(interval._samples + interval._num_samples,
                       bucket.data + num_samples, sizeof(uint32_t) * copy_size);
    }
    // 剩余按概率存入
    for (size_t i = 0; i < num_samples; ++i) {
      auto index = ::butil::fast_rand() %
                   static_cast<uint64_t>((interval._num_added + i) * ratio + 1);
      if (index < SAMPLE_SIZE) {
        interval._samples[index] = bucket.data[i];
      }
    }
    interval._num_samples = SAMPLE_SIZE;
  }
  interval._num_added += num_added;
  global_samples._num_added += num_added;
}

BABYLON_NAMESPACE_END
