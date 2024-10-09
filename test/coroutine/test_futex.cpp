#include "babylon/coroutine/futex.h"
#include "babylon/executor.h"

#include "gtest/gtest.h"

#include <future>

#if __cpp_concepts && __cpp_lib_coroutine

using ::babylon::coroutine::Futex;
using ::babylon::Executor;
using ::babylon::VersionedValue;

struct CoroutineFutexTest : public ::testing::Test {
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
  Futex futex;
};

TEST_F(CoroutineFutexTest, proxy_to_inner_awaitable) {
}

#endif // __cpp_concepts && __cpp_lib_coroutine
