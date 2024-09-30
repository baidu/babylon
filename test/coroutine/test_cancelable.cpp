#include "babylon/coroutine/cancelable.h"
#include "babylon/executor.h"

#if __cpp_concepts && __cpp_lib_coroutine

#if __cpp_lib_concepts
#include "coro/coro.hpp"
#endif // __cpp_lib_concepts

#include "gtest/gtest.h"

#include <future>

using ::babylon::CoroutineTask;
using ::babylon::Executor;
using ::babylon::VersionedValue;
using ::babylon::coroutine::Cancellable;
using ::babylon::coroutine::Cancellation;

struct CoroutineCancelableTest : public ::testing::Test {
  virtual void SetUp() override {
    executor.start();
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

  ::babylon::ThreadPoolExecutor executor;
};

TEST_F(CoroutineCancelableTest, proxy_to_inner_awaitable) {
  auto future = executor.execute([]() -> CoroutineTask<::std::string> {
    co_return *co_await Cancellable<CoroutineTask<::std::string>> {
        []() -> CoroutineTask<::std::string> {
          co_return "10086";
        }()};
  });
  ASSERT_EQ("10086", future.get());
}

TEST_F(CoroutineCancelableTest, cancel_before_finish) {
  ::std::promise<void> promise;
  ::std::promise<Cancellation> cancel_promise;
  auto future = executor.execute([&]() -> CoroutineTask<bool> {
    co_return co_await Cancellable<CoroutineTask<::std::string>> {
        [&]() -> CoroutineTask<::std::string> {
          promise.get_future().get();
          co_return "10086";
        }()}
        .on_suspend([&](Cancellation token) {
          cancel_promise.set_value(token);
        });
  });
  auto token = cancel_promise.get_future().get();
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_TRUE(token());
  promise.set_value();
  ASSERT_FALSE(future.get());
}

TEST_F(CoroutineCancelableTest, cancel_after_finish) {
  ::std::promise<void> promise;
  ::std::promise<Cancellation> cancel_promise;
  auto future = executor.execute([&]() -> CoroutineTask<::std::string> {
    co_return *co_await Cancellable<CoroutineTask<::std::string>> {
        [&]() -> CoroutineTask<::std::string> {
          promise.get_future().get();
          co_return "10086";
        }()}
        .on_suspend([&](Cancellation&& token) {
          cancel_promise.set_value(::std::move(token));
        });
  });
  auto token = cancel_promise.get_future().get();
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  promise.set_value();
  ASSERT_EQ("10086", future.get());
  ASSERT_FALSE(token());
}

TEST_F(CoroutineCancelableTest, support_void) {
  executor
      .execute([]() -> CoroutineTask<> {
        co_return *co_await Cancellable<CoroutineTask<>> {
            []() -> CoroutineTask<> {
              co_return;
            }()};
      })
      .get();
  auto future = executor.execute([]() -> CoroutineTask<bool> {
    co_return co_await Cancellable<CoroutineTask<>> {
        []() -> CoroutineTask<> {
          co_return;
        }()};
  });
  ASSERT_TRUE(future.get());
  future = executor.execute([]() -> CoroutineTask<bool> {
    co_return co_await Cancellable<CoroutineTask<>> {
        []() -> CoroutineTask<> {
          co_return;
        }()}
        .on_suspend([](Cancellation token) {
          token();
        });
  });
  ASSERT_FALSE(future.get());
}

TEST_F(CoroutineCancelableTest, switch_between_executor_correctly) {
  ::babylon::ThreadPoolExecutor executor2;
  executor2.start();

  auto future = executor.execute([&]() -> CoroutineTask<::std::string> {
    assert_in_executor(executor);
    auto result = *co_await Cancellable<CoroutineTask<::std::string>> {
        [&]() -> CoroutineTask<::std::string> {
          assert_in_executor(executor2);
          co_return "10086";
        }()
                     .set_executor(executor2)};
    assert_in_executor(executor);
    co_return result;
  });
  ASSERT_EQ("10086", future.get());
}

TEST_F(CoroutineCancelableTest, cancel_to_executor_correctly) {
  ::babylon::ThreadPoolExecutor executor2;
  executor2.start();

  ::std::promise<void> promise;
  ::std::promise<Cancellation> cancel_promise;
  auto future = executor.execute([&]() -> CoroutineTask<bool> {
    assert_in_executor(executor);
    auto result =
        co_await Cancellable<CoroutineTask<::std::string>> {
            [&]() -> CoroutineTask<::std::string> {
              assert_in_executor(executor2);
              promise.get_future().get();
              co_return "10086";
            }()
                         .set_executor(executor2)}
            .on_suspend([&](Cancellation token) {
              assert_in_executor(executor);
              cancel_promise.set_value(token);
            });
    assert_in_executor(executor);
    co_return result;
  });
  auto token = cancel_promise.get_future().get();
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_TRUE(token());
  promise.set_value();
  ASSERT_FALSE(future.get());
}
#endif // __cpp_concepts && __cpp_lib_coroutine
