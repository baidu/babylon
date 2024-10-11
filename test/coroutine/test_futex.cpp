#include "babylon/coroutine/futex.h"
#include "babylon/executor.h"
#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <future>
#include <random>

#if __cpp_concepts && __cpp_lib_coroutine

using ::babylon::Executor;
using ::babylon::coroutine::Futex;
using ::babylon::coroutine::Task;
using Cancellation = ::babylon::coroutine::Futex::Cancellation;

struct CoroutineFutexTest : public ::testing::Test {
  virtual void SetUp() override {
    futex.value() = 0;
  }

  static void assert_in_executor(Executor& executor) {
    if (!executor.is_running_in()) {
      ::abort();
    }
  }

  static void assert_not_in_executor(Executor& executor) {
    if (executor.is_running_in()) {
      ::abort();
    }
  }

  ::babylon::AlwaysUseNewThreadExecutor& executor {
      ::babylon::AlwaysUseNewThreadExecutor::instance()};
  Futex futex;
};

TEST_F(CoroutineFutexTest, do_not_suspend_if_value_not_match) {
  executor
      .execute([&]() -> Task<> {
        co_return co_await futex.wait(10086);
      })
      .get();
}

TEST_F(CoroutineFutexTest, empty_futex_wakeup_nothing) {
  ASSERT_EQ(0, futex.wake_one());
  ASSERT_EQ(0, futex.wake_all());
}

TEST_F(CoroutineFutexTest, wait_if_value_match_until_wakeup) {
  futex.value() = 10086;
  auto future = executor.execute([&]() -> Task<> {
    assert_in_executor(executor);
    co_await futex.wait(10086);
    assert_in_executor(executor);
    co_return;
  });
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_EQ(1, futex.wake_one());
  future.get();
  ASSERT_EQ(0, futex.wake_one());

  future = executor.execute([&]() -> Task<> {
    assert_in_executor(executor);
    co_await futex.wait(10086);
    assert_in_executor(executor);
    co_return;
  });
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_EQ(1, futex.wake_all());
  future.get();
  ASSERT_EQ(0, futex.wake_all());
}

TEST_F(CoroutineFutexTest, wakeup_in_reverse_order) {
  futex.value() = 10086;
  auto future1 = executor.execute([&]() -> Task<> {
    co_return co_await futex.wait(10086);
  });
  ASSERT_FALSE(future1.wait_for(::std::chrono::milliseconds {100}));
  auto future2 = executor.execute([&]() -> Task<> {
    co_return co_await futex.wait(10086);
  });
  ASSERT_FALSE(future2.wait_for(::std::chrono::milliseconds {100}));
  auto future3 = executor.execute([&]() -> Task<> {
    co_return co_await futex.wait(10086);
  });
  ASSERT_FALSE(future3.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_EQ(1, futex.wake_one());
  future3.get();
  ASSERT_EQ(1, futex.wake_one());
  future2.get();
  ASSERT_EQ(1, futex.wake_one());
  future1.get();
  ASSERT_EQ(0, futex.wake_one());
}

TEST_F(CoroutineFutexTest, wake_all_wakeup_as_many_as_possible) {
  futex.value() = 10086;
  auto future1 = executor.execute([&]() -> Task<> {
    co_return co_await futex.wait(10086);
  });
  auto future2 = executor.execute([&]() -> Task<> {
    co_return co_await futex.wait(10086);
  });
  auto future3 = executor.execute([&]() -> Task<> {
    co_return co_await futex.wait(10086);
  });
  ASSERT_FALSE(future1.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_FALSE(future2.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_FALSE(future3.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_EQ(3, futex.wake_all());
  future1.get();
  future2.get();
  future3.get();
  ASSERT_EQ(0, futex.wake_all());
}

TEST_F(CoroutineFutexTest, cancel_before_wakeup) {
  ::std::promise<Cancellation> promise;
  futex.value() = 10086;
  auto future = executor.execute([&]() -> Task<> {
    assert_in_executor(executor);
    co_await futex.wait(10086).on_suspend([&](Cancellation token) {
      promise.set_value(token);
    });
    assert_in_executor(executor);
    co_return;
  });
  auto token = promise.get_future().get();
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_TRUE(token());
  ASSERT_FALSE(token());
  future.get();
  ASSERT_EQ(0, futex.wake_one());
  ASSERT_EQ(0, futex.wake_all());
}

TEST_F(CoroutineFutexTest, cancel_adjust_waiter_list_correctly) {
  ::std::vector<::std::promise<Cancellation>> promises {100};
  ::std::vector<Cancellation> tokens {promises.size()};
  futex.value() = 10086;
  for (size_t i = 0; i < promises.size(); i++) {
    executor.execute([&, i]() -> Task<> {
      co_return co_await futex.wait(10086).on_suspend(
          [&, i](Cancellation token) {
            promises[i].set_value(token);
          });
    });
  }
  for (size_t i = 0; i < promises.size(); i++) {
    tokens[i] = promises[i].get_future().get();
  }
  ::std::mt19937 gen {::std::random_device {}()};
  ::std::shuffle(tokens.begin(), tokens.end(), gen);
  tokens.resize(tokens.size() / 2);
  for (auto token : tokens) {
    ASSERT_TRUE(token());
  }
  ASSERT_EQ(promises.size() - tokens.size(), futex.wake_all());
}

TEST_F(CoroutineFutexTest, cancel_on_suspend) {
  futex.value() = 10086;
  auto future = executor.execute([&]() -> Task<> {
    assert_in_executor(executor);
    co_await futex.wait(10086).on_suspend([&](Cancellation token) {
      token();
    });
    assert_in_executor(executor);
    co_return;
  });
  future.get();
  ASSERT_EQ(0, futex.wake_one());
}

TEST_F(CoroutineFutexTest, cancel_after_wakeup) {
  {
    ::std::promise<Cancellation> promise;
    futex.value() = 10086;
    auto future = executor.execute([&]() -> Task<> {
      assert_in_executor(executor);
      co_await futex.wait(10086).on_suspend([&](Cancellation token) {
        promise.set_value(token);
      });
      assert_in_executor(executor);
      co_return;
    });
    auto token = promise.get_future().get();
    ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
    ASSERT_EQ(1, futex.wake_one());
    ASSERT_EQ(0, futex.wake_one());
    future.get();
    ASSERT_FALSE(token());
  }
  {
    ::std::promise<Cancellation> promise;
    futex.value() = 10086;
    auto future = executor.execute([&]() -> Task<> {
      assert_in_executor(executor);
      co_await futex.wait(10086).on_suspend([&](Cancellation token) {
        promise.set_value(token);
      });
      assert_in_executor(executor);
      co_return;
    });
    auto token = promise.get_future().get();
    ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
    ASSERT_EQ(1, futex.wake_all());
    ASSERT_EQ(0, futex.wake_all());
    future.get();
    ASSERT_FALSE(token());
  }
}

TEST_F(CoroutineFutexTest, concurrent_wakeup_and_cancel) {
  ::std::vector<::std::promise<Cancellation>> promises {100};
  ::std::vector<Cancellation> tokens {promises.size()};
  futex.value() = 10086;
  for (size_t i = 0; i < promises.size(); i++) {
    executor.execute([&, i]() -> Task<> {
      co_return co_await futex.wait(10086).on_suspend(
          [&, i](Cancellation token) {
            promises[i].set_value(token);
          });
    });
  }
  for (size_t i = 0; i < promises.size(); i++) {
    tokens[i] = promises[i].get_future().get();
  }
  ::std::atomic<size_t> canceled {0};
  ::std::atomic<size_t> wake_by_one {0};
  ::std::atomic<size_t> wake_by_all {0};
  for (auto token : tokens) {
    executor.submit([&, token] {
      ::usleep(100000);
      wake_by_one += futex.wake_one();
      if (token()) {
        canceled++;
      }
      if (wake_by_one + canceled > tokens.size() / 3) {
        wake_by_all += futex.wake_all();
      }
    });
  }
  executor.join();
  BABYLON_LOG(INFO) << "canceled " << canceled << " wake_by_one " << wake_by_one
                    << " wake_by_all " << wake_by_all;
  ASSERT_EQ(promises.size(), canceled + wake_by_one + wake_by_all);
}

#endif // __cpp_concepts && __cpp_lib_coroutine
