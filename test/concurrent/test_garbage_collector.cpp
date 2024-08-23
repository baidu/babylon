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
        : _promise {::std::move(promise)} {}

    void operator()() noexcept {
      times++;
      _promise.set_value();
    }

    static size_t times;

    ::std::promise<void> _promise;
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

#if !_LIBCPP_VERSION && !ABSL_HAVE_THREAD_SANITIZER

TEST_F(GarbageCollectorTest, accessor_block_further_reclaim) {
  ::std::promise<void> p1;
  auto f1 = p1.get_future();
  ::std::promise<void> p2;
  auto f2 = p2.get_future();

  auto accessor = gc.epoch().create_accessor();
  gc.start();
  gc.retire(::std::move(p1));
  {
    ::std::unique_lock<Accessor> lock {accessor};
    gc.retire(::std::move(p2));
    f1.get();
    ASSERT_EQ(::std::future_status::timeout,
              f2.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(1, Reclaimer::times);
  }
  f2.get();
  ASSERT_EQ(2, Reclaimer::times);
  gc.stop();
}

TEST_F(GarbageCollectorTest, thread_local_accessor_block_further_reclaim) {
  ::std::promise<void> p1;
  auto f1 = p1.get_future();
  ::std::promise<void> p2;
  auto f2 = p2.get_future();

  gc.start();
  gc.retire(::std::move(p1));
  {
    ::std::unique_lock<Epoch> lock {gc.epoch()};
    gc.retire(::std::move(p2));
    f1.get();
    ASSERT_EQ(::std::future_status::timeout,
              f2.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(1, Reclaimer::times);
  }
  f2.get();
  ASSERT_EQ(2, Reclaimer::times);
  gc.stop();
}

TEST_F(GarbageCollectorTest, block_retire_when_queue_overflow) {
  ::std::promise<void> p1;
  auto f1 = p1.get_future();

  gc.set_queue_capacity(128);
  gc.start();

  ::std::thread t;
  {
    ::std::unique_lock<Epoch> lock {gc.epoch()};
    t = ::std::thread([&] {
      auto lowest_epoch = gc.epoch().tick();
      for (size_t i = 0; i < 2048; ++i) {
        gc.retire({}, lowest_epoch);
      }
      p1.set_value();
    });
    ASSERT_EQ(::std::future_status::timeout,
              f1.wait_for(::std::chrono::milliseconds(100)));
  }

  f1.get();
  t.join();
  gc.stop();
  ASSERT_EQ(2048, Reclaimer::times);
}
#endif // !_LIBCPP_VERSION && !ABSL_HAVE_THREAD_SANITIZER
