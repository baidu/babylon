#include "babylon/coroutine/cancelable.h"
#include "babylon/executor.h"
#include "babylon/logging/logger.h"

#if __cpp_concepts && __cpp_lib_coroutine

#include "gtest/gtest.h"

#include <future>

using ::babylon::CoroutineTask;
using ::babylon::Executor;
using ::babylon::VersionedValue;
using ::babylon::coroutine::Cancellable;
using Cancellation = ::babylon::coroutine::BasicCancellable::Cancellation;

struct CoroutineCancelableTest : public ::testing::Test {
  virtual void SetUp() override {
    executor.set_worker_number(8);
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
        [](::std::promise<void>& promise) -> CoroutineTask<::std::string> {
          promise.get_future().get();
          co_return "10086";
        }(promise)}
        .on_suspend([&](Cancellation token) {
          cancel_promise.set_value(token);
        });
  });
  auto token = cancel_promise.get_future().get();
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_TRUE(token());
  ASSERT_FALSE(future.get());
  promise.set_value();
  executor.stop();
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
    co_return co_await Cancellable<CoroutineTask<>> {[]() -> CoroutineTask<> {
      co_return;
    }()};
  });
  ASSERT_TRUE(future.get());
  future = executor.execute([]() -> CoroutineTask<bool> {
    co_return co_await Cancellable<CoroutineTask<>> {[]() -> CoroutineTask<> {
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
            [](::std::future<void> future,
               ::babylon::Executor& executor2) -> CoroutineTask<::std::string> {
              assert_in_executor(executor2);
              future.get();
              co_return "10086";
            }(promise.get_future(), executor2)
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
  ASSERT_FALSE(future.get());
  promise.set_value();
  executor2.stop();
}

TEST_F(CoroutineCancelableTest, concurrent_finish_and_cancel) {
  auto& executor2 = ::babylon::AlwaysUseNewThreadExecutor::instance();

  ::std::vector<::std::promise<Cancellation>> promises {100};
  ::std::vector<::std::future<Cancellation>> futures {promises.size()};
  for (size_t i = 0; i < promises.size(); i++) {
    futures[(i + 1) % promises.size()] = promises[i].get_future();
  }

  ::std::atomic<size_t> finished {0};
  ::std::atomic<size_t> canceled {0};
  ::babylon::CountDownLatch<> latch {promises.size()};
  for (size_t i = 0; i < promises.size(); i++) {
    executor.submit([&, i]() -> CoroutineTask<> {
      auto result =
          co_await Cancellable<CoroutineTask<::std::string>> {
              [](::std::future<Cancellation>& future)
                  -> CoroutineTask<::std::string> {
                if (future.valid()) {
                  future.get()();
                }
                co_return "10086";
              }(futures[i])
                         .set_executor(executor2)}
              .on_suspend([&, i](Cancellation token) {
                promises[i].set_value(token);
              });
      if (result) {
        finished++;
      } else {
        canceled++;
      }
      latch.count_down();
    });
  }
  latch.get_future().get();
  executor2.join();
  executor.stop();
  BABYLON_LOG(INFO) << "finished " << finished << " canceled " << canceled;
}

#endif // __cpp_concepts && __cpp_lib_coroutine
