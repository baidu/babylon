#include "babylon/concurrent/garbage_collector.h"

#include "gtest/gtest.h"

#include <future>

using ::babylon::Epoch;
using ::babylon::GarbageCollector;
using Accessor = Epoch::Accessor;

struct GarbageCollectorTest : public ::testing::Test {
  struct Reclaimer {
    Reclaimer() = default;
    Reclaimer(Reclaimer&&) = default;
    Reclaimer& operator=(Reclaimer&&) = default;

    Reclaimer(::std::promise<void>&& promise)
        : promise {::std::move(promise)} {}

    void operator()() noexcept {
      times++;
      promise.set_value();
    }

    static size_t times;

    ::std::promise<void> promise;
  };

  virtual void SetUp() override {
    Reclaimer::times = 0;
  }

  GarbageCollector<Reclaimer> gc;
};

size_t GarbageCollectorTest::Reclaimer::times {0};

TEST_F(GarbageCollectorTest, reclaim_if_no_accessor) {
  gc.start();
  gc.retire({});
  gc.stop();
  ASSERT_EQ(1, Reclaimer::times);
}

TEST_F(GarbageCollectorTest, wait_reclaim_on_destroy) {
  {
    GarbageCollector<Reclaimer> gc;
    gc.start();
    gc.retire({});
  }
  ASSERT_EQ(1, Reclaimer::times);
}

TEST_F(GarbageCollectorTest, accessor_block_further_reclaim) {
  auto accessor = gc.epoch().create_accessor();
  gc.start();
  gc.retire({});
  ::std::promise<void> promise;
  auto future = promise.get_future();
  {
    ::std::unique_lock<Accessor> lock {accessor};
    gc.retire(::std::move(promise));
    ASSERT_EQ(::std::future_status::timeout,
              future.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(1, Reclaimer::times);
  }
  future.get();
  ASSERT_EQ(2, Reclaimer::times);
  gc.stop();
}

TEST_F(GarbageCollectorTest, thread_local_accessor_block_further_reclaim) {
  gc.start();
  gc.retire({});
  ::std::promise<void> promise;
  auto future = promise.get_future();
  {
    ::std::unique_lock<Epoch> lock {gc.epoch()};
    gc.retire(::std::move(promise));
    ASSERT_EQ(::std::future_status::timeout,
              future.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(1, Reclaimer::times);
  }
  future.get();
  ASSERT_EQ(2, Reclaimer::times);
  gc.stop();
}
