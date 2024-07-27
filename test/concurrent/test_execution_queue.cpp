#include "babylon/concurrent/execution_queue.h"

#include "gtest/gtest.h"

#include <future>

using ::babylon::AlwaysUseNewThreadExecutor;
using ::babylon::ConcurrentExecutionQueue;
using ::babylon::Executor;
using ::babylon::InplaceExecutor;
using ::babylon::MoveOnlyFunction;
using Queue = ConcurrentExecutionQueue<::std::string>;
using Iterator = Queue::Iterator;

TEST(concurrent_execution_queue, initialize_reserve_queue_capacity) {
  ConcurrentExecutionQueue<::std::string> queue;
  auto ret = queue.initialize(2, AlwaysUseNewThreadExecutor::instance(),
                              [](Iterator, Iterator) {});
  ASSERT_EQ(0, ret);
  ASSERT_EQ(2, queue.capacity());
  ret = queue.initialize(5, AlwaysUseNewThreadExecutor::instance(),
                         [](Iterator, Iterator) {});
  ASSERT_EQ(0, ret);
  ASSERT_EQ(8, queue.capacity());
  ret = queue.initialize(8, AlwaysUseNewThreadExecutor::instance(),
                         [](Iterator, Iterator) {});
  ASSERT_EQ(0, ret);
  ASSERT_EQ(8, queue.capacity());
}

TEST(concurrent_execution_queue, execute_submit_execute_async_but_serially) {
  ConcurrentExecutionQueue<::std::string> queue;
  ::std::vector<::std::string> vector;
  ::std::promise<void> promise;
  auto future = promise.get_future();
  auto ret =
      queue.initialize(4, AlwaysUseNewThreadExecutor::instance(),
                       [&](Iterator begin, Iterator end) {
                         if (future.valid()) {
                           future.get();
                         }
                         ::std::copy(begin, end, ::std::back_inserter(vector));
                       });
  ASSERT_EQ(0, ret);
  ASSERT_EQ(0, queue.execute("10086"));
  ASSERT_EQ(0, queue.execute("10010"));
  ASSERT_NE(2, vector.size());
  promise.set_value();
  queue.join();
  ASSERT_EQ(2, vector.size());
  ASSERT_EQ("10086", vector[0]);
  ASSERT_EQ("10010", vector[1]);
}

TEST(concurrent_execution_queue, execute_inplace) {
  ConcurrentExecutionQueue<::std::string> queue;
  ::std::vector<::std::string> vector;
  auto ret = queue.initialize(
      4, InplaceExecutor::instance(), [&](Iterator begin, Iterator end) {
        ::std::copy(begin, end, ::std::back_inserter(vector));
      });
  ASSERT_EQ(0, ret);
  ASSERT_EQ(0, queue.execute("10086"));
  ASSERT_EQ(1, vector.size());
  ASSERT_EQ(0, queue.execute("10010"));
  ASSERT_EQ(2, vector.size());
  queue.join();
}

TEST(concurrent_execution_queue, execute_fail_when_launch_consumer_error) {
  struct E : public Executor {
    virtual int invoke(
        MoveOnlyFunction<void(void)>&& function) noexcept override {
      if (times++ == 0) {
        return -1;
      }
      function();
      return 0;
    }
    size_t times = 0;
  } executor;
  ConcurrentExecutionQueue<::std::string> queue;
  bool called = false;
  auto ret = queue.initialize(4, executor, [&](Iterator, Iterator) {
    called = true;
  });
  ASSERT_EQ(0, ret);
  ASSERT_NE(0, queue.execute("10086"));
  queue.join();
  ASSERT_FALSE(called);
  ASSERT_EQ(0, queue.execute("10086"));
  queue.join();
  ASSERT_TRUE(called);
}
