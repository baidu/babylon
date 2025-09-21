#include <babylon/concurrent/counter.h>

#include <gtest/gtest.h>

#include <thread>

using ::babylon::ConcurrentAdder;
using ::babylon::ConcurrentMaxer;
using ::babylon::ConcurrentMiner;
using ::babylon::ConcurrentSampler;
using ::babylon::ConcurrentSummer;

template <typename T>
void test_concurrent_adder_caculate_right() {
  ::babylon::GenericsConcurrentAdder<T> adder;
  ::std::thread a([&] {
    adder << 10;
  });
  ::std::thread b([&] {
    adder << 3;
  });
  ::std::thread c([&] {
    adder << -5;
  });
  a.join();
  b.join();
  c.join();
  ASSERT_EQ(8, adder.value());
}

TEST(concurrent_adder, caculate_right) {
  // 保证编译兼容性。
  ConcurrentAdder adder;

  test_concurrent_adder_caculate_right<ssize_t>();
  test_concurrent_adder_caculate_right<int64_t>();
  test_concurrent_adder_caculate_right<uint64_t>();
  test_concurrent_adder_caculate_right<int32_t>();
  test_concurrent_adder_caculate_right<uint32_t>();
}

template <typename T>
void test_concurrent_maxer_caculate_right() {
  ::babylon::GenericsConcurrentMaxer<T> maxer;
  ::std::thread a([&] {
    maxer << 10;
  });
  ::std::thread b([&] {
    maxer << 3;
  });
  ::std::thread c([&] {
    maxer << -5;
  });
  a.join();
  b.join();
  c.join();
  ASSERT_EQ(10, maxer.value());
}

TEST(concurrent_maxer, caculate_right) {
  // 保证编译兼容性。
  ConcurrentMaxer maxer;

  test_concurrent_maxer_caculate_right<ssize_t>();
  test_concurrent_maxer_caculate_right<int64_t>();
  test_concurrent_maxer_caculate_right<int32_t>();
}

template <typename T>
void test_concurrent_maxer_empty_aware() {
  ::babylon::GenericsConcurrentMaxer<T> maxer;
  T value = 10086;
  auto has_value = maxer.value(value);
  ASSERT_FALSE(has_value);
  ASSERT_EQ(10086, value);
  ASSERT_EQ(0, maxer.value());

  ::std::thread([&] {
    maxer << 10;
  }).join();

  has_value = maxer.value(value);
  ASSERT_TRUE(has_value);
  ASSERT_EQ(10, value);
  ASSERT_EQ(10, maxer.value());

  maxer.reset();

  value = 10010;
  has_value = maxer.value(value);
  ASSERT_FALSE(has_value);
  ASSERT_EQ(10010, value);
  ASSERT_EQ(0, maxer.value());
}

TEST(concurrent_maxer, empty_aware) {
  // 保证编译兼容性。
  ConcurrentMaxer maxer;

  test_concurrent_maxer_empty_aware<ssize_t>();
  test_concurrent_maxer_empty_aware<int64_t>();
  test_concurrent_maxer_empty_aware<uint64_t>();
  test_concurrent_maxer_empty_aware<int32_t>();
  test_concurrent_maxer_empty_aware<uint32_t>();
}

template <typename T>
void test_concurrent_maxer_resetable() {
  ::babylon::GenericsConcurrentMaxer<T> maxer;
  {
    ::std::thread a([&] {
      maxer << 10;
    });
    ::std::thread b([&] {
      maxer << 3;
    });
    ::std::thread c([&] {
      maxer << -5;
    });
    a.join();
    b.join();
    c.join();
  }
  ASSERT_EQ(10, maxer.value());
  maxer.reset();
  {
    ::std::thread a([&] {
      maxer << 3;
    });
    ::std::thread b([&] {
      maxer << 7;
    });
    ::std::thread c([&] {
      maxer << -2;
    });
    a.join();
    b.join();
    c.join();
  }
  ASSERT_EQ(7, maxer.value());
}

TEST(concurrent_maxer, resetable) {
  // 保证编译兼容性。
  ConcurrentMaxer maxer;

  test_concurrent_maxer_resetable<ssize_t>();
  test_concurrent_maxer_resetable<int64_t>();
  test_concurrent_maxer_resetable<int32_t>();
}

template <typename T>
void test_concurrent_miner_caculate_right() {
  ::babylon::GenericsConcurrentMiner<T> miner;
  ::std::thread a([&] {
    miner << 10;
  });
  ::std::thread b([&] {
    miner << 3;
  });
  ::std::thread c([&] {
    miner << -5;
  });
  a.join();
  b.join();
  c.join();
  ASSERT_EQ(-5, miner.value());
}

TEST(concurrent_miner, caculate_right) {
  // 保证编译兼容性。
  ConcurrentMiner miner;

  test_concurrent_miner_caculate_right<ssize_t>();
  test_concurrent_miner_caculate_right<int64_t>();
  test_concurrent_miner_caculate_right<int32_t>();
}

template <typename T>
void test_concurrent_miner_empty_aware() {
  ::babylon::GenericsConcurrentMiner<T> miner;
  T value = 10086;
  auto has_value = miner.value(value);
  ASSERT_FALSE(has_value);
  ASSERT_EQ(10086, value);
  ASSERT_EQ(0, miner.value());
  ::std::thread([&] {
    miner << 10;
  }).join();
  has_value = miner.value(value);
  ASSERT_TRUE(has_value);
  ASSERT_EQ(10, value);
  ASSERT_EQ(10, miner.value());
  miner.reset();
  value = 10010;
  has_value = miner.value(value);
  ASSERT_FALSE(has_value);
  ASSERT_EQ(10010, value);
  ASSERT_EQ(0, miner.value());
}

TEST(concurrent_miner, empty_aware) {
  // 保证编译兼容性。
  ConcurrentMiner miner;

  test_concurrent_miner_empty_aware<ssize_t>();
  test_concurrent_miner_empty_aware<int64_t>();
  test_concurrent_miner_empty_aware<int32_t>();
}

template <typename T>
void test_concurrent_miner_resetable() {
  ::babylon::GenericsConcurrentMiner<T> miner;
  {
    ::std::thread a([&] {
      miner << 10;
    });
    ::std::thread b([&] {
      miner << 3;
    });
    ::std::thread c([&] {
      miner << -5;
    });
    a.join();
    b.join();
    c.join();
  }
  ASSERT_EQ(-5, miner.value());
  miner.reset();
  {
    ::std::thread a([&] {
      miner << 3;
    });
    ::std::thread b([&] {
      miner << 7;
    });
    ::std::thread c([&] {
      miner << -2;
    });
    a.join();
    b.join();
    c.join();
  }
  ASSERT_EQ(-2, miner.value());
}

TEST(concurrent_miner, resetable) {
  // 保证编译兼容性。
  ConcurrentMiner miner;

  test_concurrent_miner_resetable<ssize_t>();
  test_concurrent_miner_resetable<int64_t>();
  test_concurrent_miner_resetable<int32_t>();
}

TEST(concurrent_summary, caculate_right) {
  ConcurrentSummer summer;
  ::std::thread a([&] {
    summer << 10;
  });
  ::std::thread b([&] {
    summer << 3;
  });
  ::std::thread c([&] {
    summer.operator<<({-5, 5});
  });
  a.join();
  b.join();
  c.join();
  ASSERT_EQ(8, summer.value().sum);
  ASSERT_EQ(7, summer.value().num);
}

TEST(concurrent_sampler, collect_sample_from_multithread) {
  ConcurrentSampler sampler;
  ::std::thread a([&] {
    sampler << 1;
    sampler << 3;
  });
  ::std::thread b([&] {
    sampler << 3;
    sampler << 5;
  });
  ::std::thread c([&] {
    sampler << 7;
    sampler << 9;
  });
  a.join();
  b.join();
  c.join();

  size_t total_record_num = 0;
  ::std::vector<uint32_t> result;
  sampler.for_each([&](size_t, const ConcurrentSampler::SampleBucket& bucket) {
    auto record_num = bucket.record_num.load(::std::memory_order_acquire);
    total_record_num += record_num;
    record_num = ::std::min<size_t>(record_num, bucket.capacity);
    for (size_t i = 0; i < record_num; ++i) {
      result.push_back(bucket.data[i]);
    }
  });

  ::std::vector<uint32_t> expected = {1, 3, 3, 5, 7, 9};
  ::std::sort(result.begin(), result.end());
  ASSERT_EQ(result.size(), total_record_num);
  ASSERT_EQ(expected, result);
}

TEST(concurrent_sampler, random_drop_sample_after_reach_capacity) {
  ConcurrentSampler sampler;
  for (size_t i = 0; i < 50; ++i) {
    sampler << 1;
  }

  size_t total_record_num = 0;
  ::std::vector<uint32_t> result;
  sampler.for_each([&](size_t, const ConcurrentSampler::SampleBucket& bucket) {
    auto record_num = bucket.record_num.load(::std::memory_order_acquire);
    total_record_num += record_num;
    record_num = ::std::min<size_t>(record_num, bucket.capacity);
    for (size_t i = 0; i < record_num; ++i) {
      result.push_back(bucket.data[i]);
    }
  });

  ASSERT_EQ(50, total_record_num);
  ASSERT_EQ(30, result.size());
}

TEST(concurrent_sampler, reset_drops_all) {
  ConcurrentSampler sampler;
  for (size_t i = 0; i < 50; ++i) {
    sampler << 1;
  }

  sampler.reset();
  sampler << 10086;

  size_t total_record_num = 0;
  ::std::vector<uint32_t> result;
  sampler.for_each([&](size_t, const ConcurrentSampler::SampleBucket& bucket) {
    auto record_num = bucket.record_num.load(::std::memory_order_acquire);
    total_record_num += record_num;
    record_num = ::std::min<size_t>(record_num, bucket.capacity);
    for (size_t i = 0; i < record_num; ++i) {
      result.push_back(bucket.data[i]);
    }
  });

  ASSERT_EQ(1, total_record_num);
  ASSERT_EQ(1, result.size());
  ASSERT_EQ(10086, result[0]);
}

TEST(concurrent_sampler, new_capacity_used_after_reset) {
  ConcurrentSampler sampler;
  sampler << 100;
  sampler.set_bucket_capacity(ConcurrentSampler::bucket_index(100), 100);
  for (size_t i = 0; i < 49; ++i) {
    sampler << 100;
  }

  {
    size_t total_record_num = 0;
    ::std::vector<uint32_t> result;
    sampler.for_each(
        [&](size_t, const ConcurrentSampler::SampleBucket& bucket) {
          auto record_num = bucket.record_num.load(::std::memory_order_acquire);
          total_record_num += record_num;
          record_num = ::std::min<size_t>(record_num, bucket.capacity);
          for (size_t i = 0; i < record_num; ++i) {
            result.push_back(bucket.data[i]);
          }
        });
    ASSERT_EQ(50, total_record_num);
    ASSERT_EQ(30, result.size());
  }

  sampler.reset();
  for (size_t i = 0; i < 50; ++i) {
    sampler << 100;
  }

  {
    size_t total_record_num = 0;
    ::std::vector<uint32_t> result;
    sampler.for_each(
        [&](size_t, const ConcurrentSampler::SampleBucket& bucket) {
          auto record_num = bucket.record_num.load(::std::memory_order_acquire);
          total_record_num += record_num;
          record_num = ::std::min<size_t>(record_num, bucket.capacity);
          for (size_t i = 0; i < record_num; ++i) {
            result.push_back(bucket.data[i]);
          }
        });
    ASSERT_EQ(50, total_record_num);
    ASSERT_EQ(50, result.size());
  }
}
