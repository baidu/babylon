#include <babylon/future.h>

#include <gtest/gtest.h>

#include <future>
#include <random>
#include <thread>

using ::babylon::CountDownLatch;
using ::babylon::Future;
using ::babylon::Promise;

TEST(future, future_create_by_default_not_valid) {
  Future<int> future;
  ASSERT_FALSE(future.valid());
  ASSERT_FALSE(future);
  ASSERT_FALSE(future.ready());
}

TEST(future, value_set_by_promise_can_get_by_future_related) {
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    ASSERT_FALSE(future.ready());
    promise.set_value(10086);
    ASSERT_TRUE(future.ready());
    ASSERT_EQ(10086, future.get());
  }
  {
    Promise<int32_t> promise;
    promise.set_value(10086);
    auto future = promise.get_future();
    ASSERT_TRUE(future.ready());
    ASSERT_EQ(10086, future.get());
  }
}

TEST(future, get_wait_until_ready) {
  Promise<int> promise;
  auto future = promise.get_future();
  ::std::mutex mutex;
  mutex.lock();
  ::std::thread([&] {
    { ::std::lock_guard<::std::mutex> lock {mutex}; }
    promise.set_value(10086);
  }).detach();
  ASSERT_FALSE(future.ready());
  mutex.unlock();
  ASSERT_EQ(10086, future.get());
  ASSERT_TRUE(future.ready());
}

TEST(future, wait_for_may_timeout) {
  Promise<void> promise;
  auto future = promise.get_future();
  ::std::mutex mutex;
  mutex.lock();
  ::std::thread([&] {
    { ::std::lock_guard<::std::mutex> lock {mutex}; }
    promise.set_value();
  }).detach();
  ASSERT_FALSE(future.wait_for(std::chrono::milliseconds(100)));
  mutex.unlock();
  ASSERT_TRUE(future.wait_for(std::chrono::milliseconds(100)));
  ASSERT_TRUE(future.ready());
}

TEST(future, wait_for_very_long_time_is_ok) {
  Promise<void> promise;
  auto future = promise.get_future();
  ::std::thread([&] {
    ::usleep(100000);
    promise.set_value();
  }).detach();
  ASSERT_TRUE(future.wait_for(std::chrono::nanoseconds(INT64_MAX)));
  ASSERT_TRUE(future.ready());
}

TEST(future, wait_for_very_short_time_is_ok) {
  Promise<void> promise;
  auto future = promise.get_future();
  ::std::thread([&] {
    ::usleep(100000);
    promise.set_value();
  }).detach();
  ASSERT_FALSE(future.wait_for(std::chrono::nanoseconds(0)));
  ASSERT_FALSE(future.wait_for(std::chrono::nanoseconds(1)));
  ASSERT_FALSE(future.ready());
  future.get();
  ASSERT_TRUE(future.wait_for(std::chrono::nanoseconds(0)));
  ASSERT_TRUE(future.wait_for(std::chrono::nanoseconds(1)));
}

TEST(future, wait_for_treat_negative_timeout_as_zero) {
  Promise<void> promise;
  auto future = promise.get_future();
  ::std::thread([&] {
    ::usleep(100000);
    promise.set_value();
  }).detach();
  ASSERT_FALSE(future.wait_for(
      std::chrono::nanoseconds(static_cast<uint64_t>(INT64_MAX) + 1)));
  ASSERT_FALSE(future.wait_for(std::chrono::nanoseconds(-1)));
  ASSERT_FALSE(future.ready());
  future.get();
  ASSERT_TRUE(future.wait_for(
      std::chrono::nanoseconds(static_cast<uint64_t>(INT64_MAX) + 1)));
  ASSERT_TRUE(future.wait_for(std::chrono::nanoseconds(-1)));
}

TEST(future, on_finish_before_ready_called_with_value_when_ready) {
  Promise<int32_t> promise;
  auto future = promise.get_future();
  bool called = false;
  int32_t value_pass_to_callback = 0;
  future.on_finish([&](int32_t value) {
    value_pass_to_callback = value;
    called = true;
  });
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(called);
  promise.set_value(10086);
  ASSERT_TRUE(called);
  ASSERT_EQ(10086, value_pass_to_callback);
  ASSERT_EQ(10086, future.get());
}

TEST(future, on_finish_after_ready_called_inplace) {
  Promise<int32_t> promise;
  auto future = promise.get_future();
  promise.set_value(10086);
  bool called = false;
  int32_t value_pass_to_callback = 0;
  future.on_finish([&](int32_t value) {
    value_pass_to_callback = value;
    called = true;
  });
  // 已经ready时原地执行callback，此时已经执行完了
  ASSERT_TRUE(called);
  ASSERT_EQ(10086, value_pass_to_callback);
}

TEST(future, on_finish_accept_callback_in_different_form) {
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    bool called = false;
    int32_t value_pass_to_callback = 0;
    future.on_finish([&](int32_t& value) {
      value_pass_to_callback = value;
      called = true;
    });
    ASSERT_FALSE(called);
    promise.set_value(10086);
    ASSERT_TRUE(called);
    ASSERT_EQ(10086, value_pass_to_callback);
  }
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    bool called = false;
    future.on_finish([&] {
      called = true;
    });
    ASSERT_FALSE(called);
    promise.set_value(10086);
    ASSERT_TRUE(called);
  }
}

TEST(future, future_work_with_void) {
  ::std::mutex mutex;
  {
    Promise<void> promise;
    auto future = promise.get_future();
    mutex.lock();
    ::std::thread thread([&] {
      { ::std::lock_guard<::std::mutex> lock {mutex}; }
      promise.set_value();
    });
    ASSERT_FALSE(future.ready());
    mutex.unlock();
    future.get();
    ASSERT_TRUE(future.ready());
    thread.join();
  }
  {
    Promise<void> promise;
    auto future = promise.get_future();
    bool called = false;
    future.on_finish([&] {
      called = true;
    });
    ASSERT_FALSE(called);
    promise.set_value();
    ASSERT_TRUE(called);
  }
}

TEST(future, future_destory_without_wait) {
  Promise<int32_t> promise;
  {
    auto future = promise.get_future();
    // 析构并不等待
  }
  // 赋值并没有问题，只是没有感知者了
  promise.set_value(10086);
}

TEST(future, moved_future_works_fine) {
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    decltype(future) moved_future(::std::move(future));
    ASSERT_FALSE(future);
    promise.set_value(10086);
    ASSERT_EQ(10086, moved_future.get());
  }
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    decltype(future) moved_future;
    moved_future = ::std::move(future);
    ASSERT_FALSE(future);
    promise.set_value(10086);
    ASSERT_EQ(10086, moved_future.get());
  }
}

TEST(future, copyed_future_work_with_same_promise) {
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    decltype(future) copyed_future(future);
    ASSERT_TRUE(future);
    ASSERT_TRUE(copyed_future);
    promise.set_value(10086);
    ASSERT_EQ(10086, future.get());
    ASSERT_EQ(10086, copyed_future.get());
    ASSERT_EQ(&future.get(), &copyed_future.get());
  }
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    decltype(future) copyed_future;
    copyed_future = future;
    ASSERT_TRUE(future);
    ASSERT_TRUE(copyed_future);
    promise.set_value(10086);
    ASSERT_EQ(10086, future.get());
    ASSERT_EQ(10086, copyed_future.get());
    ASSERT_EQ(&future.get(), &copyed_future.get());
  }
}

TEST(future, can_register_callback) {
  Promise<int> promise;
  auto future = promise.get_future();
  int value_in_callback = 0;
  future.on_finish([&](int x) {
    value_in_callback = x;
  });
  ASSERT_EQ(0, value_in_callback);
  promise.set_value(10086);
  ASSERT_EQ(10086, value_in_callback);
}

TEST(future, callback_can_build_a_chain) {
  Promise<int32_t> promise;
  auto future = promise.get_future();
  int32_t value_in_then_future = 0;
  auto then_future = future
                         .then([](int32_t x) {
                           return x + 1;
                         })
                         .then([&](int32_t x) {
                           value_in_then_future = x + 1;
                         });
  auto x = [] {};
  auto final_future = then_future.then(x);
  promise.set_value(123);
  ASSERT_EQ(125, value_in_then_future);
  ASSERT_TRUE(final_future.ready());
}

TEST(future, support_lvalue_reference) {
  Promise<int&> promise;
  auto future = promise.get_future();

  int x = 10010;
  promise.set_value(x);
  future.get() = 10086;
  ASSERT_EQ(10086, x);
}

TEST(future, concurrent_works_fine) {
  size_t times = 10;
  size_t concurrent = 32;

  ::std::mt19937 gen(::std::random_device {}());
  ::std::vector<size_t> step_one_value;
  ::std::vector<size_t> step_two_value;
  ::std::vector<::std::string> results;
  for (size_t i = 0; i < concurrent; ++i) {
    step_one_value.emplace_back(gen());
    step_two_value.emplace_back(gen());
    results.emplace_back(::std::to_string(step_one_value[i]) +
                         ::std::to_string(step_two_value[i]));
  }

  for (size_t j = 0; j < times; ++j) {
    ::std::vector<::std::thread> threads;
    ::std::vector<Promise<::std::string>> promises;
    ::std::vector<Future<::std::string>> futures;
    threads.reserve(concurrent * 2);
    promises.resize(concurrent);
    futures.resize(concurrent);
    ::std::atomic<size_t> ready_num(0);
    for (size_t i = 0; i < concurrent; ++i) {
      auto& promise = promises[i];
      futures[i] = promise.get_future();
      auto& future = futures[i];
      threads.emplace_back([&, i] {
        ready_num++;
        while (ready_num != concurrent) {
          ::sched_yield();
        }
        promise.set_value(::std::to_string(step_one_value[i]));
      });
      threads.emplace_back([&, i] {
        future.get().append(::std::to_string(step_two_value[i]));
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
    for (size_t i = 0; i < concurrent; ++i) {
      auto& s = promises[i].get_future().get();
      ASSERT_EQ(results[i], s);
    }
  }
}

TEST(promise, promise_set_value_with_emplace_semantics) {
  struct S {
    S(int32_t v) : v(v) {}
    S() = delete;
    S(S&&) = delete;
    S(const S&) = delete;
    int32_t v;
  };
  // 不需要默认构造
  Promise<S> promise;
  auto future = promise.get_future();
  ASSERT_FALSE(future.ready());
  // 使用S::S(10086)原地构造
  promise.set_value(10086);
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10086, future.get().v);
}

TEST(promise, moved_promise_works_fine) {
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    decltype(promise) moved_promise(::std::move(promise));
    moved_promise.set_value(10086);
    ASSERT_EQ(10086, future.get());
  }
  {
    Promise<int32_t> promise;
    auto future = promise.get_future();
    decltype(promise) moved_promise;
    moved_promise = ::std::move(promise);
    moved_promise.set_value(10086);
    ASSERT_EQ(10086, future.get());
  }
}

TEST(promise, cleared_promise_before_set_never_invoke_callback) {
  bool callback_invoked = false;
  Promise<int> promise;
  auto future = promise.get_future();
  future.on_finish([&](int&) {
    callback_invoked = true;
  });
  promise.clear();
  ASSERT_FALSE(callback_invoked);
}

TEST(promise, promise_without_future_do_not_need_set_value) {
  Promise<int32_t> promise;
  auto future = promise.get_future();
}

TEST(promise, report_double_set_but_dont_crash) {
  Promise<::std::string> promise;
  auto future = promise.get_future();
  promise.set_value("10086");
#ifdef NDEBUG
  promise.set_value("10010");
#endif // NDEBUG
  ASSERT_EQ("10086", future.get());
}

TEST(promise, destroy_value_after_both_promise_and_future_destroy) {
  struct S {
    ~S() {
      destructor_called() = true;
    }
    static bool& destructor_called() {
      static bool called = false;
      return called;
    }
  };
  S::destructor_called() = false;
  {
    Promise<S> promise;
    {
      auto future = promise.get_future();
      promise.set_value();
    }
    ASSERT_FALSE(S::destructor_called());
  }
  ASSERT_TRUE(S::destructor_called());
  S::destructor_called() = false;
  {
    Promise<S> promise;
    promise.set_value();
    ASSERT_FALSE(S::destructor_called());
  }
  ASSERT_TRUE(S::destructor_called());
  S::destructor_called() = false;
  {
    Future<S> future;
    {
      Promise<S> promise;
      future = promise.get_future();
      promise.set_value();
    }
    ASSERT_FALSE(S::destructor_called());
  }
  ASSERT_TRUE(S::destructor_called());
}

TEST(promise, skip_value_destroy_while_value_not_set) {
  struct S {
    ~S() {
      destructor_called() = true;
    }
    static bool& destructor_called() {
      static bool called = false;
      return called;
    }
  };
  S::destructor_called() = false;
  {
    Promise<S> promise;
    { promise.get_future(); }
    ASSERT_FALSE(S::destructor_called());
  }
  ASSERT_FALSE(S::destructor_called());
}

TEST(promise, reusable_after_clear) {
  Promise<::std::string> promise;
  {
    auto future = promise.get_future();
    ASSERT_FALSE(future.ready());
    ::std::thread([&] {
      usleep(100000);
      promise.set_value("10086");
    }).detach();
    ASSERT_EQ("10086", future.get());
  }
  ASSERT_TRUE(promise.get_future().ready());
  promise.clear();
  ASSERT_FALSE(promise.get_future().ready());
  {
    auto future = promise.get_future();
    ASSERT_FALSE(future.ready());
    ::std::thread([&] {
      usleep(100000);
      promise.set_value("10010");
    }).detach();
    ASSERT_EQ("10010", future.get());
  }
}

TEST(latch, notice_future_when_count_to_sero) {
  CountDownLatch<> latch(10);
  auto future = latch.get_future();
  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < 10; ++i) {
    threads.emplace_back([&] {
      latch.count_down();
    });
  }
  future.get();
  for (auto& thread : threads) {
    thread.join();
  }
}

#ifdef NDEBUG
TEST(latch, destroy_before_count_down_to_zero_never_notice_future) {
  Future<size_t> future;
  {
    CountDownLatch<> latch(10);
    future = latch.get_future();
    latch.count_down(8);
  }
  ASSERT_FALSE(future.ready());
}
#endif // NDEBUG

TEST(latch, do_not_need_count_down_to_zero_without_future) {
  CountDownLatch<> latch(10);
  auto future = latch.get_future();
  latch.count_down(8);
}

TEST(latch, already_finished_when_construct_with_zero_count) {
  CountDownLatch<> latch(0);
  auto future = latch.get_future();
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  future.get();
  // 穿透的count down不会出错但是无意义
  latch.count_down();
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  future.get();

  Promise<size_t> p;
  Future<size_t> f = p.get_future();
  p.set_value(1);
  f.get();
}
