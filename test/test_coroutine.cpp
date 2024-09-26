#include "babylon/executor.h"

#if __cpp_concepts && __cpp_lib_coroutine

#if __cpp_lib_concepts
#include "coro/coro.hpp"
#endif // __cpp_lib_concepts

#include "gtest/gtest.h"

#include <future>

using ::babylon::CoroutineTask;
using ::babylon::Executor;

struct CoroutineTest : public ::testing::Test {
  virtual void SetUp() override {
    executor.set_worker_number(8);
    executor.set_local_capacity(8);
    executor.start();
    destroy_times = 0;
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

  template <typename T>
  struct Deleter {
    void operator()(T* ptr) noexcept {
      destroy_times++;
      delete ptr;
    }
  };
  template <typename T>
  using P = ::std::unique_ptr<T, Deleter<T>>;

  static size_t destroy_times;
};
size_t CoroutineTest::destroy_times {0};

TEST_F(CoroutineTest, default_destroy_with_task) {
  using S = P<int>;
  {
    auto task = [](S) -> CoroutineTask<> {
      co_return;
    }(S {new int});
    ASSERT_EQ(0, destroy_times);
  }
  ASSERT_EQ(1, destroy_times);
}

TEST_F(CoroutineTest, task_detach_coroutine_after_submit) {
  using S = P<::std::future<void>>;
  ::std::promise<void> promise;
  ::babylon::Future<void> future;
  {
    auto task = [](S s) -> CoroutineTask<> {
      s->get();
      co_return;
    }(S {new ::std::future<void> {promise.get_future()}});
    future = executor.execute(::std::move(task));
    ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  }
  ASSERT_EQ(0, destroy_times);
  promise.set_value();
  future.get();
  executor.stop();
  ASSERT_EQ(1, destroy_times);
}

TEST_F(CoroutineTest, coroutine_awaiter_destroy_after_awaitee_resume_it) {
  using S = P<::std::future<void>>;
  ::std::promise<void> promise;
  ::babylon::Future<void> future;
  {
    auto task = [](S s) -> CoroutineTask<> {
      co_await [](S s) -> CoroutineTask<> {
        s->get();
        co_return;
      }(::std::move(s));
    }(S {new ::std::future<void> {promise.get_future()}});
    future = executor.execute(::std::move(task));
    ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  }
  ASSERT_EQ(0, destroy_times);
  promise.set_value();
  future.get();
  executor.stop();
  ASSERT_EQ(1, destroy_times);
}

TEST_F(CoroutineTest, coroutine_execute_and_resume_in_executor_they_belong) {
  ::babylon::ThreadPoolExecutor executor2;
  executor2.set_worker_number(8);
  executor2.set_local_capacity(8);
  executor2.start();

  executor
      .execute([&]() -> CoroutineTask<> {
        assert_in_executor(executor);
        co_await [&]() -> CoroutineTask<> {
          assert_in_executor(executor);
          co_return;
        }();
        assert_in_executor(executor);
        co_await [&]() -> CoroutineTask<> {
          assert_in_executor(executor2);
          co_return;
        }()
                              .set_executor(executor2);
        assert_in_executor(executor);
      })
      .get();
}

TEST_F(CoroutineTest, future_is_awaitable) {
  ::babylon::Promise<::std::string> promise;
  auto future = executor.execute(
      [future =
           promise.get_future()]() mutable -> CoroutineTask<::std::string> {
        co_return co_await ::std::move(future);
      });
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  promise.set_value("10086");
  ASSERT_EQ("10086", future.get());
}

TEST_F(CoroutineTest, future_is_shared_awaitable) {
  ::babylon::Promise<::std::string> promise;
  auto future1 = executor.execute(
      [future =
           promise.get_future()]() mutable -> CoroutineTask<::std::string> {
        co_return co_await future;
      });
  auto future2 = executor.execute(
      [future =
           promise.get_future()]() mutable -> CoroutineTask<::std::string> {
        co_return co_await future;
      });
  ASSERT_FALSE(future1.wait_for(::std::chrono::milliseconds {100}));
  ASSERT_FALSE(future2.wait_for(::std::chrono::milliseconds {100}));
  promise.set_value("10086");
  ASSERT_EQ("10086", future1.get());
  ASSERT_EQ("10086", future2.get());
}

#if __cpp_lib_concepts
TEST_F(CoroutineTest, non_babylon_coroutine_task_is_awaitable) {
  ::coro::thread_pool pool {::coro::thread_pool::options {.thread_count = 1}};
  ::std::promise<::std::string> promise;
  auto future = executor.execute(
      [&](::std::future<::std::string> future) -> CoroutineTask<::std::string> {
        co_return co_await [&](::std::future<::std::string> future)
                               -> coro::task<::std::string> {
          co_await pool.schedule();
          co_return future.get();
        }(::std::move(future));
      },
      promise.get_future());
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  promise.set_value("10086");
  ASSERT_EQ("10086", future.get());

  // auto l = []() -> CoroutineTask<> {
  //   co_return;
  // };
  // using X = Executor::AwaitResultType<decltype(l)>;
}

TEST_F(CoroutineTest, awaitable_by_non_babylon_coroutine_task) {
  ::coro::thread_pool pool {::coro::thread_pool::options {.thread_count = 1}};
  ::std::promise<::std::string> promise;
  auto task =
      [&](::std::future<::std::string> future) -> coro::task<::std::string> {
    co_await pool.schedule();
    co_return co_await [&](::std::future<::std::string> future)
                           -> CoroutineTask<::std::string> {
      co_return future.get();
    }(::std::move(future));
  };
  promise.set_value("10086");
  ASSERT_EQ("10086", ::coro::sync_wait(task(promise.get_future())));
}

TEST_F(CoroutineTest, future_awaitable_by_non_babylon_coroutine_task) {
  ::coro::thread_pool pool {::coro::thread_pool::options {.thread_count = 1}};
  ::babylon::Promise<::std::string> promise;
  auto task = [&](::babylon::Future<::std::string> future)
      -> coro::task<::std::string> {
    co_await pool.schedule();
    co_return co_await ::babylon::FutureAwaitable<::std::string>(
        ::std::move(future));
  };
  promise.set_value("10086");
  ASSERT_EQ("10086", ::coro::sync_wait(task(promise.get_future())));
}

TEST_F(CoroutineTest, return_to_executor_when_resume_by_non_babylon_coroutine) {
  ::coro::thread_pool pool {::coro::thread_pool::options {.thread_count = 1}};
  executor
      .execute([&]() -> CoroutineTask<> {
        assert_in_executor(executor);
        co_await [&]() -> coro::task<> {
          assert_in_executor(executor);
          co_await pool.schedule();
          assert_not_in_executor(executor);
          co_await [&]() -> CoroutineTask<> {
            assert_in_executor(executor);
            co_return;
          }()
                                .set_executor(executor);
          assert_in_executor(executor);
          co_await pool.schedule();
          assert_not_in_executor(executor);
        }();
        assert_in_executor(executor);
      })
      .get();
}
#endif // __cpp_lib_concepts

#endif // __cpp_concepts && __cpp_lib_coroutine
