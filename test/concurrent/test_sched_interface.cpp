#include "babylon/concurrent/sched_interface.h"

// clang-format off
#include "babylon/protect.h"
// clang-format on

#include "gtest/gtest.h"

#include <future> // ::std::async

using ::babylon::Futex;
using ::babylon::SchedInterface;

struct NeedCreateSchedInterface : public SchedInterface {
  constexpr static bool futex_need_create() noexcept {
    return true;
  }
};

TEST(futex_interface, create_destroy_work_fine_even_not_necessary) {
  ASSERT_FALSE(SchedInterface::futex_need_create());
  auto* futex = SchedInterface::create_futex();
  ASSERT_NE(nullptr, futex);
  *futex = 10086;
  SchedInterface::destroy_futex(futex);
}

TEST(futex_interface, wait_return_if_not_equal) {
  {
    uint32_t futex = 10086;
    ASSERT_NE(0, SchedInterface::futex_wait(&futex, 10087, nullptr));
    ASSERT_EQ(EAGAIN, errno);
  }
  {
    Futex<SchedInterface> futex(10086);
    ASSERT_NE(0, futex.wait(10087, nullptr));
    ASSERT_EQ(EAGAIN, errno);
  }
  {
    Futex<NeedCreateSchedInterface> futex(10086);
    ASSERT_NE(0, futex.wait(10087, nullptr));
    ASSERT_EQ(EAGAIN, errno);
  }
}

TEST(futex_interface, wait_return_if_timeout) {
  struct ::timespec timeout = {.tv_sec = 0, .tv_nsec = 10000000};
  {
    uint32_t futex = 10086;
    ASSERT_NE(0, SchedInterface::futex_wait(&futex, 10086, &timeout));
    ASSERT_EQ(ETIMEDOUT, errno);
  }
  {
    Futex<SchedInterface> futex = 10086;
    ASSERT_NE(0, futex.wait(10086, &timeout));
    ASSERT_EQ(ETIMEDOUT, errno);
  }
  {
    Futex<NeedCreateSchedInterface> futex = 10086;
    ASSERT_NE(0, futex.wait(10086, &timeout));
    ASSERT_EQ(ETIMEDOUT, errno);
  }
}

TEST(futex_interface, wait_can_be_wakeup) {
  {
    uint32_t futex = 0;
    ASSERT_EQ(0, SchedInterface::futex_wake_one(&futex));
    auto future = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, SchedInterface::futex_wait(&futex, 0, nullptr));
    });
    ASSERT_EQ(::std::future_status::timeout,
              future.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(1, SchedInterface::futex_wake_one(&futex));
  }
  {
    Futex<SchedInterface> futex = 0;
    ASSERT_EQ(0, futex.wake_one());
    auto future = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, futex.wait(0, nullptr));
    });
    ASSERT_EQ(::std::future_status::timeout,
              future.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(1, futex.wake_one());
  }
  {
    Futex<NeedCreateSchedInterface> futex = 0;
    ASSERT_EQ(0, futex.wake_one());
    auto future = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, futex.wait(0, nullptr));
    });
    ASSERT_EQ(::std::future_status::timeout,
              future.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(1, futex.wake_one());
  }
}

TEST(futex_interface, waiter_wakeup_one_by_one) {
  ::std::atomic<uint32_t> futex {0};
  ::std::atomic<int> running {2};
  ::std::promise<void> promise;
  auto future = promise.get_future();
  auto wait_func = [&] {
    while (futex.load(::std::memory_order_relaxed) == 0) {
      SchedInterface::futex_wait(&reinterpret_cast<uint32_t&>(futex), 0,
                                 nullptr);
    }
    if (running.fetch_sub(1, ::std::memory_order_relaxed) == 0) {
      promise.set_value();
    }
  };
  auto f1 = ::std::async(::std::launch::async, wait_func);
  auto f2 = ::std::async(::std::launch::async, wait_func);
  ASSERT_EQ(::std::future_status::timeout,
            future.wait_for(::std::chrono::milliseconds(100)));
  ASSERT_EQ(2, running.load(::std::memory_order_relaxed));
  futex.store(1, ::std::memory_order_relaxed);
  ASSERT_EQ(
      1, SchedInterface::futex_wake_one(&reinterpret_cast<uint32_t&>(futex)));
  ASSERT_EQ(::std::future_status::timeout,
            future.wait_for(::std::chrono::milliseconds(100)));
  ASSERT_EQ(1, running.load(::std::memory_order_relaxed));
  ASSERT_EQ(
      1, SchedInterface::futex_wake_one(&reinterpret_cast<uint32_t&>(futex)));
  // ASSERT_EQ(::std::future_status::ready,
  // future.wait_for(::std::chrono::milliseconds(100)));
  f1.get();
  f2.get();
  ASSERT_EQ(0, running.load(::std::memory_order_relaxed));
}

TEST(futex_interface, waiter_wakeup_all) {
  {
    uint32_t futex = 0;
    auto f1 = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, SchedInterface::futex_wait(&futex, 0, nullptr));
    });
    auto f2 = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, SchedInterface::futex_wait(&futex, 0, nullptr));
    });
    ASSERT_EQ(::std::future_status::timeout,
              f1.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(2, SchedInterface::futex_wake_all(&futex));
  }
  {
    Futex<SchedInterface> futex = 0;
    auto f1 = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, futex.wait(0, nullptr));
    });
    auto f2 = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, futex.wait(0, nullptr));
    });
    ASSERT_EQ(::std::future_status::timeout,
              f1.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(2, futex.wake_all());
  }
  {
    Futex<NeedCreateSchedInterface> futex = 0;
    auto f1 = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, futex.wait(0, nullptr));
    });
    auto f2 = ::std::async(std::launch::async, [&] {
      ASSERT_EQ(0, futex.wait(0, nullptr));
    });
    ASSERT_EQ(::std::future_status::timeout,
              f1.wait_for(::std::chrono::milliseconds(100)));
    ASSERT_EQ(2, futex.wake_all());
  }
}

TEST(futex_interface, futex_copy_with_value) {
  {
    Futex<SchedInterface> futex(10086);
    const Futex<SchedInterface> futex_copyed(futex);
    ASSERT_EQ(10086, futex_copyed.value().load());
    Futex<SchedInterface> futex_assigned;
    futex_assigned = futex;
    ASSERT_EQ(10086, futex_assigned.value().load());
  }
  {
    Futex<NeedCreateSchedInterface> futex(10086);
    const Futex<NeedCreateSchedInterface> futex_copyed(futex);
    ASSERT_EQ(10086, futex_copyed.value().load());
    Futex<NeedCreateSchedInterface> futex_assigned;
    futex_assigned = futex;
    ASSERT_EQ(10086, futex_assigned.value().load());
  }
}

TEST(futex_interface, futex_value_read_write_as_atomic) {
  {
    Futex<SchedInterface> futex(10086);
    ASSERT_EQ(10086, futex.value().fetch_add(1));
    ASSERT_EQ(10087, futex.value().load());
  }
  {
    Futex<NeedCreateSchedInterface> futex(10086);
    ASSERT_EQ(10086, futex.value().fetch_add(1));
    ASSERT_EQ(10087, futex.value().load());
  }
}

#include "babylon/unprotect.h"
