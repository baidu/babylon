#include "babylon/executor.h"

#if __cpp_concepts && __cpp_lib_coroutine

#if __cpp_lib_concepts
#include "coro/coro.hpp"
#endif // __cpp_lib_concepts

#include "gtest/gtest.h"

#include <future>

using ::babylon::CoroutineTask;

struct CoroutineTest : public ::testing::Test {
  virtual void SetUp() override {
    executor.set_worker_number(8);
    executor.set_local_capacity(8);
    executor.start();
    destroy_times = 0;
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
  using IT = ::std::tuple<::std::future<void>, ::babylon::Executor*>;
  using S = P<IT>;
  using OT = ::std::tuple<::babylon::Executor*, ::babylon::Executor*,
                          ::babylon::Executor*>;
  ::babylon::ThreadPoolExecutor executor2;
  executor2.set_worker_number(8);
  executor2.set_local_capacity(8);
  executor2.start();

  ::std::promise<void> promise;
  ::babylon::Future<OT> future;
  auto task = [](S s) -> CoroutineTask<OT> {
    auto e0 = ::babylon::CurrentExecutor::get();
    auto executor2 = ::std::get<1>(*s);
    auto awaitee = [](S s) -> CoroutineTask<::babylon::Executor*> {
      ::std::get<0>(*s).get();
      co_return ::babylon::CurrentExecutor::get();
    }(::std::move(s));
    awaitee.set_executor(*executor2);
    auto e1 = co_await ::std::move(awaitee);
    auto e2 = ::babylon::CurrentExecutor::get();
    co_return OT {e0, e1, e2};
  }(S {new IT {promise.get_future(), &executor2}});
  future = executor.execute(::std::move(task));
  promise.set_value();
  auto t = future.get();
  ASSERT_EQ(&executor, ::std::get<0>(t));
  ASSERT_EQ(&executor2, ::std::get<1>(t));
  ASSERT_EQ(&executor, ::std::get<2>(t));
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
  ::std::array<::babylon::Executor*, 7> e;
  auto future = executor.execute([&]() -> CoroutineTask<> {
    e[0] = ::babylon::CurrentExecutor::get();
    co_await [&]() -> coro::task<> {
      e[1] = ::babylon::CurrentExecutor::get();
      co_await pool.schedule();
      e[2] = ::babylon::CurrentExecutor::get();
      co_await [&]() -> CoroutineTask<> {
        e[3] = ::babylon::CurrentExecutor::get();
        co_return;
      }()
                            .set_executor(executor);
      e[4] = ::babylon::CurrentExecutor::get();
      co_await pool.schedule();
      e[5] = ::babylon::CurrentExecutor::get();
    }();
    e[6] = ::babylon::CurrentExecutor::get();
  });
  future.get();
  ASSERT_EQ(&executor, e[0]);
  ASSERT_EQ(&executor, e[1]);
  ASSERT_EQ(nullptr, e[2]);
  ASSERT_EQ(&executor, e[3]);
  ASSERT_EQ(&executor, e[4]);
  ASSERT_EQ(nullptr, e[5]);
  ASSERT_EQ(&executor, e[6]);
}
#endif // __cpp_lib_concepts

#endif // __cpp_concepts && __cpp_lib_coroutine
