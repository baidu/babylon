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
    ASSERT_TRUE(task);
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
    ASSERT_FALSE(task);
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
    ASSERT_FALSE(task);
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

#if __cpp_lib_concepts
TEST_F(CoroutineTest, non_babylon_coroutine_task_is_awaitable) {
  ::coro::thread_pool pool {::coro::thread_pool::options {.thread_count = 1}};

  ::std::promise<::std::string> promise;
  auto future = promise.get_future();
  auto c = [&pool, future = ::std::move(future)]() mutable -> coro::task<::std::string> {
    co_await pool.schedule();
    co_return future.get();
  };

  auto t = c();
  ::coro::sync_wait(c());
  // auto future = executor.execute([&]() -> CoroutineTask<::std::string> {

  //    });
  // ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  // promise.set_value("10086");
  // ASSERT_EQ("10086", future.get());
}
#endif // __cpp_lib_concepts

#endif // __cpp_concepts && __cpp_lib_coroutine
