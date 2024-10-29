#include "babylon/coroutine/task.h"
#include "babylon/coroutine/yield_awaitable.h"
#include "babylon/executor.h"
#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <future>

#if __cpp_concepts && __cpp_lib_coroutine

using ::babylon::Executor;
using ::babylon::coroutine::Task;

struct CoroutineYieldTest : public ::testing::Test {
  virtual void SetUp() override {
    executor.set_worker_number(8);
    executor.set_local_capacity(8);
    executor.set_global_capacity(8);
    executor.start();
  }

  static void assert_in_executor(Executor& executor) {
    if (!executor.is_running_in()) {
      ::abort();
    }
  }

  ::babylon::ThreadPoolExecutor executor;
};

TEST_F(CoroutineYieldTest, yield_let_pending_task_run_first) {
  ::std::promise<void> step1_promise;
  ::std::promise<void> step2_promise;
  ::std::promise<::babylon::Future<void>> sub_promise;
  auto future = executor.execute([&]() -> Task<> {
    sub_promise.set_value(executor.execute([&]() -> Task<> {
      co_return;
    }));
    step1_promise.get_future().get();
    co_await ::babylon::coroutine::yield();
    assert_in_executor(executor);
    step2_promise.get_future().get();
  });
  auto sub_future = sub_promise.get_future().get();
  ASSERT_FALSE(sub_future.wait_for(::std::chrono::milliseconds {100}));
  step1_promise.set_value();
  sub_future.get();
  ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
  step2_promise.set_value();
  future.get();
}

TEST_F(CoroutineYieldTest, yield_can_be_forced_non_inplace) {
  ::std::promise<void> step1_promise;
  ::std::promise<void> step2_promise;
  ::std::promise<::babylon::Future<void>> sub_promise;
  auto future = executor.execute([&]() -> Task<> {
    sub_promise.set_value(executor.execute([&]() -> Task<> {
      co_await ::babylon::coroutine::yield().set_non_inplace();
      assert_in_executor(executor);
    }));
    step1_promise.get_future().get();
    co_await ::babylon::coroutine::yield();
    assert_in_executor(executor);
    step2_promise.get_future().get();
  });
  auto sub_future = sub_promise.get_future().get();
  step1_promise.set_value();
  sub_future.get();
  step2_promise.set_value();
  future.get();
}

#endif // __cpp_concepts && __cpp_lib_coroutine
