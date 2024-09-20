#include "babylon/concurrent/epoch.h"
#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>
#include <thread>

#pragma GCC diagnostic ignored "-Wswitch-default" // ASSERT_DEATH

using ::babylon::Epoch;
using Accessor = ::babylon::Epoch::Accessor;

struct EpochTest : public ::testing::Test {
  Epoch epoch;
};

TEST_F(EpochTest, default_accessor_not_valid) {
  Accessor accessor;
  ASSERT_FALSE(accessor);
  ASSERT_DEATH(accessor.lock(), "");
}

TEST_F(EpochTest, accessor_valid_until_release) {
  auto accessor = epoch.create_accessor();
  ASSERT_TRUE(accessor);
  { ::std::lock_guard<Accessor> lock {accessor}; }
  accessor.release();
  ASSERT_FALSE(accessor);
  accessor.release();
  ASSERT_FALSE(accessor);
  ASSERT_DEATH(accessor.lock(), "");
}

TEST_F(EpochTest, accessor_auto_release_when_destruct) {
  ASSERT_EQ(0, epoch.accessor_number());
  {
    auto accessor = epoch.create_accessor();
    ASSERT_EQ(1, epoch.accessor_number());
  }
  {
    auto accessor = epoch.create_accessor();
    ASSERT_EQ(1, epoch.accessor_number());
    auto accessor2 = epoch.create_accessor();
    ASSERT_EQ(2, epoch.accessor_number());
  }
}

TEST_F(EpochTest, accessor_movable) {
  auto accessor = epoch.create_accessor();
  {
    auto accessor_moved = ::std::move(accessor);
    { ::std::lock_guard<Accessor> lock {accessor_moved}; }
    ASSERT_DEATH(accessor.lock(), "");

    accessor = ::std::move(accessor_moved);
    { ::std::lock_guard<Accessor> lock {accessor}; }
    ASSERT_DEATH(accessor_moved.lock(), "");
  }
  { ::std::lock_guard<Accessor> lock {accessor}; }
}

TEST_F(EpochTest, epoch_increase_when_tick) {
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(i + 1, epoch.tick());
  }
}

TEST_F(EpochTest, low_water_mark_count_lowest_accessor_locked) {
  ::std::vector<Accessor> accessors;
  for (size_t i = 0; i < 10; ++i) {
    accessors.emplace_back(epoch.create_accessor());
  }
  ASSERT_EQ(UINT64_MAX, epoch.low_water_mark());
  for (size_t i = 0; i < 5; ++i) {
    accessors[i].lock();
    epoch.tick();
  }
  ASSERT_EQ(0, epoch.low_water_mark());
  for (size_t i = 0; i < 4; ++i) {
    accessors[i].unlock();
    ASSERT_EQ(i + 1, epoch.low_water_mark());
  }
  accessors[4].unlock();
  ASSERT_EQ(UINT64_MAX, epoch.low_water_mark());
}

TEST_F(EpochTest, concurrent_works_fine) {
  ::std::atomic<::std::string*> ptr {nullptr};
  ::std::random_device rd;

  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < 128; ++i) {
    threads.emplace_back([&] {
      ::std::mt19937 gen {rd()};
      size_t sum = 0;
      for (size_t i = 0; i < 10000; ++i) {
        if (gen() % 2 == 0) {
          auto accessor = epoch.create_accessor();
          ::std::lock_guard<Accessor> lock {accessor};
          auto s = ptr.load(::std::memory_order_acquire);
          sum += s != nullptr ? ::std::stol(*s) : 0;
        } else {
          auto s = new ::std::string(::std::to_string(gen()));
          auto old_s = ptr.exchange(s, ::std::memory_order_acq_rel);
          auto reclaim_version = epoch.tick();
          sum += old_s != nullptr ? ::std::stol(*old_s) : 0;
          while (reclaim_version > epoch.low_water_mark()) {
            ::sched_yield();
          }
          delete old_s;
        }
      }
      BABYLON_LOG(INFO) << "sum = " << sum;
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  delete ptr;
}

TEST_F(EpochTest, concurrent_works_fine_in_thread_local_style) {
  ::std::atomic<::std::string*> ptr {nullptr};
  ::std::random_device rd;

  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < 128; ++i) {
    threads.emplace_back([&] {
      ::std::mt19937 gen {rd()};
      size_t sum = 0;
      for (size_t i = 0; i < 10000; ++i) {
        if (gen() % 2 == 0) {
          ::std::lock_guard<Epoch> lock {epoch};
          auto s = ptr.load(::std::memory_order_acquire);
          sum += s != nullptr ? ::std::stol(*s) : 0;
        } else {
          auto s = new ::std::string(::std::to_string(gen()));
          auto old_s = ptr.exchange(s, ::std::memory_order_acq_rel);
          auto reclaim_version = epoch.tick();
          sum += old_s != nullptr ? ::std::stol(*old_s) : 0;
          while (reclaim_version > epoch.low_water_mark()) {
            ::sched_yield();
          }
          delete old_s;
        }
      }
      BABYLON_LOG(INFO) << "sum = " << sum;
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  delete ptr;
}
