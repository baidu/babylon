#include "babylon/executor.h"

#include "gtest/gtest.h"

#include <future>

using ::babylon::AlwaysUseNewThreadExecutor;
using ::babylon::async;
using ::babylon::Executor;
using ::babylon::Future;
using ::babylon::InplaceExecutor;
using ::babylon::MoveOnlyFunction;
using ::babylon::ThreadPoolExecutor;

struct ExecutorTest : public ::testing::Test {
  InplaceExecutor& inplace_executor = InplaceExecutor::instance();
  AlwaysUseNewThreadExecutor& thread_executor =
      AlwaysUseNewThreadExecutor::instance();
};

TEST_F(ExecutorTest, can_execute_normal_function) {
  struct S {
    static int function(int i) {
      return value() += i;
    }
    static int& value() {
      static int v = 1;
      return v;
    }
  };
  auto future = inplace_executor.execute(S::function, 10086);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10087, future.get());
  future = async(inplace_executor, S::function, 1);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10088, future.get());
  inplace_executor.submit(S::function, 1);
  ASSERT_EQ(10089, S::value());
}

TEST_F(ExecutorTest, can_execute_member_function) {
  struct S {
    int function(int i) {
      return value += i;
    }
    int value {1};
  };
  S s;
  auto future = inplace_executor.execute(&S::function, &s, 10086);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10087, future.get());
  future = async(inplace_executor, &S::function, &s, 1);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10088, future.get());
  inplace_executor.submit(&S::function, &s, 1);
  ASSERT_EQ(10089, s.value);
}

TEST_F(ExecutorTest, can_execute_function_object) {
  struct S {
    S(int i) : i(i) {}
    int operator()(int& value) {
      return value += i;
    }
    int i;
  };
  int value {1};
  auto future = inplace_executor.execute(S {10086}, ::std::ref(value));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10087, future.get());
  future = async(inplace_executor, S {1}, ::std::ref(value));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10088, future.get());
  inplace_executor.submit(S {2}, ::std::ref(value));
  ASSERT_EQ(10090, value);
}

TEST_F(ExecutorTest, can_execute_lambda) {
  int value {1};
  auto future = inplace_executor.execute(
      [&](int i) {
        return value += i;
      },
      10086);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10087, future.get());
  future = async(
      inplace_executor,
      [&](int i) {
        return value += i;
      },
      1);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10088, future.get());
  inplace_executor.submit(
      [&](int i) {
        return value += i;
      },
      1);
  ASSERT_EQ(10089, value);

  auto lambda = [&](int i) {
    return value += i;
  };
  future = inplace_executor.execute(lambda, 1);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10090, future.get());
  future = async(inplace_executor, lambda, 1);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10091, future.get());
  inplace_executor.submit(lambda, 1);
  ASSERT_EQ(10092, value);
}

TEST_F(ExecutorTest, can_execute_binded_function) {
  int value {1};
  auto future = inplace_executor.execute(::std::bind(
                                             [&](int add, int mul) {
                                               return value = value * mul + add;
                                             },
                                             1, ::std::placeholders::_1),
                                         10086);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10087, future.get());
  future = async(inplace_executor,
                 ::std::bind(
                     [&](int add, int mul) {
                       return value = value * mul + add;
                     },
                     3, ::std::placeholders::_1),
                 1);
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10090, future.get());
  inplace_executor.submit(::std::bind(
                              [&](int add, int mul) {
                                return value = value * mul + add;
                              },
                              4, ::std::placeholders::_1),
                          1);
  ASSERT_EQ(10094, value);
}

#if __cpp_concepts && __cpp_lib_coroutine
TEST_F(ExecutorTest, support_coroutine_function) {
  struct S {
    static ::babylon::CoroutineTask<::std::string> run(
        ::std::promise<::std::string> promise) {
      promise.set_value("10086");
      co_return "10086";
    }
  };
  {
    ::std::promise<::std::string> promise;
    auto future = promise.get_future();
    auto ret = thread_executor.submit(S::run, ::std::move(promise));
    ASSERT_EQ(0, ret);
    ASSERT_EQ("10086", future.get());
  }
  {
    auto future =
        thread_executor.execute(S::run, ::std::promise<::std::string> {});
    ASSERT_TRUE(future);
    ASSERT_EQ("10086", future.get());
  }
}

TEST_F(ExecutorTest, support_coroutine_member_function) {
  struct S {
    ::babylon::CoroutineTask<::std::string> run(::std::string prefix) {
      promise.set_value(prefix + "-10086");
      co_return prefix + "-10086";
    }
    ::std::promise<::std::string> promise;
  };
  {
    S s;
    auto future = s.promise.get_future();
    auto ret = thread_executor.submit(&S::run, &s, "10010");
    ASSERT_EQ(0, ret);
    ASSERT_EQ("10010-10086", future.get());
  }
  {
    S s;
    auto future = thread_executor.execute(&S::run, &s, "10010");
    ASSERT_TRUE(future);
    ASSERT_EQ("10010-10086", future.get());
  }
}

TEST_F(ExecutorTest, support_coroutine_function_object) {
  struct S {
    ::babylon::CoroutineTask<::std::string> operator()(::std::string prefix) {
      promise.set_value(prefix + "-10086");
      co_return prefix + "-10086";
    }
    ::std::promise<::std::string> promise;
  };
  {
    ::std::future<::std::string> future;
    {
      S s;
      future = s.promise.get_future();
      auto ret = thread_executor.submit(::std::move(s), "10010");
      ASSERT_EQ(0, ret);
    }
    ASSERT_EQ("10010-10086", future.get());
  }
  {
    ::babylon::Future<::std::string> future;
    {
      S s;
      future = thread_executor.execute(::std::move(s), "10010");
      ASSERT_TRUE(future);
    }
    ASSERT_EQ("10010-10086", future.get());
  }
}

TEST_F(ExecutorTest, support_coroutine_lambda) {
  {
    ::std::future<::std::string> future;
    {
      ::std::promise<::std::string> promise;
      future = promise.get_future();
      auto l = [promise = ::std::move(promise)](::std::string prefix) mutable
          -> ::babylon::CoroutineTask<::std::string> {
        promise.set_value(prefix + "-10086");
        co_return prefix + "-10086";
      };
      auto ret = thread_executor.submit(::std::move(l), "10010");
      ASSERT_EQ(0, ret);
    }
    ASSERT_EQ("10010-10086", future.get());
  }
  {
    ::babylon::Future<::std::string> future;
    {
      ::std::promise<::std::string> promise;
      auto l = [promise = ::std::move(promise)](::std::string prefix) mutable
          -> ::babylon::CoroutineTask<::std::string> {
        promise.set_value(prefix + "-10086");
        co_return prefix + "-10086";
      };
      future = thread_executor.execute(::std::move(l), "10010");
      ASSERT_TRUE(future);
    }
    ASSERT_EQ("10010-10086", future.get());
  }
}

TEST_F(ExecutorTest, support_coroutine_co_return_void) {
  struct S {
    static ::babylon::CoroutineTask<> run() {
      co_return;
    }
    ::babylon::CoroutineTask<> member_run() {
      co_return;
    }
    ::babylon::CoroutineTask<> operator()() {
      co_return;
    }
  } s;
  ::babylon::Future<void> future = thread_executor.execute(S::run);
  future.get();
  future = thread_executor.execute(&S::member_run, &s);
  future.get();
  future = thread_executor.execute(s);
  future.get();
  future = thread_executor.execute([]() -> ::babylon::CoroutineTask<> {
    co_return;
  });
  future.get();
}
#endif // __cpp_concepts && __cpp_lib_coroutine

TEST_F(ExecutorTest, handle_void_return) {
  int value {1};
  auto future = inplace_executor.execute([&] {
    ++value;
  });
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  future.get();
  ASSERT_EQ(2, value);
  future = async(inplace_executor, [&] {
    ++value;
  });
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  future.get();
  ASSERT_EQ(3, value);
  inplace_executor.submit([&] {
    ++value;
  });
  ASSERT_EQ(4, value);
}

TEST_F(ExecutorTest, handle_non_copyable_function_and_args) {
  struct S {
    S(int i) : i(i) {}
    S(const S&) = delete;
    S(S&&) = default;
    int operator()(int& value) {
      return value += i;
    }
    int i;
  };
  int value {1};
  auto future = inplace_executor.execute(S {10086}, ::std::ref(value));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10087, future.get());
  future = async(inplace_executor, S {1}, ::std::ref(value));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10088, future.get());
  inplace_executor.submit(S {2}, ::std::ref(value));
  ASSERT_EQ(10090, value);

  future = inplace_executor.execute(::std::bind(S {3}, ::std::ref(value)));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10093, future.get());
  future = async(inplace_executor, ::std::bind(S {4}, ::std::ref(value)));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10097, future.get());
  inplace_executor.submit(::std::bind(S {5}, ::std::ref(value)));
  ASSERT_EQ(10102, value);

  future = inplace_executor.execute(
      [](S&& s, int& value) {
        return s(value);
      },
      S {6}, ::std::ref(value));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10108, future.get());
  future = async(
      inplace_executor,
      [](S&& s, int& value) {
        return s(value);
      },
      S {7}, ::std::ref(value));
  ASSERT_TRUE(future.valid());
  ASSERT_TRUE(future.ready());
  ASSERT_EQ(10115, future.get());
  inplace_executor.submit(
      [](S&& s, int& value) {
        return s(value);
      },
      S {8}, ::std::ref(value));
  ASSERT_EQ(10123, value);
}

TEST_F(ExecutorTest, return_invalid_future_when_invoke_fail) {
  struct BadExecutor : public Executor {
    virtual int invoke(MoveOnlyFunction<void(void)>&&) noexcept override {
      return -1;
    }
  } executor;
  auto future = executor.execute([] {});
  ASSERT_FALSE(future.valid());
  future = async(executor, [] {});
  ASSERT_FALSE(future.valid());
  ASSERT_NE(0, executor.submit([] {}));
}

TEST_F(ExecutorTest, current_executor_mark_during_execution) {
  {
    struct S {
      static void function(Executor& e) {
        ASSERT_TRUE(e.is_running_in());
      }
      void member_function(Executor& e) {
        ASSERT_TRUE(e.is_running_in());
      }
      void operator()(Executor& e) {
        ASSERT_TRUE(e.is_running_in());
      }
    } s;
    thread_executor.execute(S::function, ::std::ref(thread_executor)).get();
    thread_executor
        .execute(&S::member_function, &s, ::std::ref(thread_executor))
        .get();
    thread_executor.execute(s, ::std::ref(thread_executor)).get();
    thread_executor
        .execute(
            [](Executor& e) {
              ASSERT_TRUE(e.is_running_in());
            },
            ::std::ref(thread_executor))
        .get();
  }
#if __cpp_concepts && __cpp_lib_coroutine
  {
    struct S {
      static ::babylon::CoroutineTask<> run(Executor& e) {
        if (!e.is_running_in()) {
          abort();
        }
        co_return;
      }
      ::babylon::CoroutineTask<> member_run(Executor& e) {
        if (!e.is_running_in()) {
          abort();
        }
        co_return;
      }
      ::babylon::CoroutineTask<> operator()(Executor& e) {
        if (!e.is_running_in()) {
          abort();
        }
        co_return;
      }
    } s;
    thread_executor.execute(S::run, thread_executor).get();
    thread_executor.execute(&S::member_run, &s, thread_executor).get();
    thread_executor.execute(s, thread_executor).get();
    thread_executor
        .execute(
            [&](Executor& e) -> ::babylon::CoroutineTask<> {
              if (!e.is_running_in()) {
                abort();
              }
              co_return;
            },
            thread_executor)
        .get();
  }
#endif // __cpp_concepts && __cpp_lib_coroutine
}

TEST_F(ExecutorTest, inplace_reentry_execution) {
  int value {1};
  int see[3] {0, 0, 0};
  inplace_executor.execute([&] {
    inplace_executor.submit([&] {
      inplace_executor.execute([&] {
        see[2] = value++;
      });
      see[1] = value++;
    });
    see[0] = value++;
  });
  ASSERT_EQ(3, see[0]);
  ASSERT_EQ(2, see[1]);
  ASSERT_EQ(1, see[2]);
}

TEST_F(ExecutorTest, run_async_in_new_thread) {
  auto lambda = [] {
    thread_local int value {1};
    ::usleep(100000);
    return ++value;
  };
  ASSERT_EQ(2, lambda());
  ASSERT_EQ(3, lambda());
  auto future = AlwaysUseNewThreadExecutor::instance().execute(lambda);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.ready());
  ASSERT_EQ(2, future.get());
  future = AlwaysUseNewThreadExecutor::instance().execute(lambda);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.ready());
  ASSERT_EQ(2, future.get());
}

TEST_F(ExecutorTest, run_async_in_thread_pool) {
  ThreadPoolExecutor executor;
  ASSERT_EQ(0, executor.start());
  auto lambda = [] {
    thread_local int value {1};
    ::usleep(10000);
    return ++value;
  };
  ASSERT_EQ(2, lambda());
  ASSERT_EQ(3, lambda());
  auto future = executor.execute(lambda);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.ready());
  ASSERT_EQ(2, future.get());
  future = executor.execute(lambda);
  ASSERT_TRUE(future.valid());
  ASSERT_FALSE(future.ready());
  ASSERT_EQ(3, future.get());
}

TEST_F(ExecutorTest, local_task_keep_stay_local_in_capacity) {
  {
    ThreadPoolExecutor executor;
    executor.set_worker_number(2);
    executor.start();
    executor
        .execute([&] {
          auto future = executor.execute([] {});
          ASSERT_TRUE(future.wait_for(::std::chrono::milliseconds {100}));
        })
        .get();
  }
  {
    ThreadPoolExecutor executor;
    executor.set_worker_number(2);
    executor.set_local_capacity(1);
    executor.start();
    ::babylon::Future<void> future;
    executor
        .execute([&] {
          future = executor.execute([] {});
          ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
        })
        .get();
    future.get();
  }
  {
    ThreadPoolExecutor executor;
    executor.set_worker_number(2);
    executor.set_local_capacity(1);
    executor.start();
    ::babylon::Future<void> future;
    executor
        .execute([&] {
          future = executor.execute([] {});
          ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
          executor.execute([] {}).get();
        })
        .get();
    future.get();
  }
}

TEST_F(ExecutorTest, local_task_auto_steal_when_finish) {
  ThreadPoolExecutor executor;
  executor.set_worker_number(2);
  executor.set_local_capacity(1);
  executor.set_enable_work_stealing(true);
  executor.start();

  ::std::promise<void> promise;
  auto future = promise.get_future();
  executor.submit([&] {
    future.get();
  });
  executor
      .execute([&] {
        auto inner_future = executor.execute([] {});
        ASSERT_FALSE(inner_future.wait_for(::std::chrono::milliseconds {100}));
        promise.set_value();
        inner_future.get();
      })
      .get();
}

TEST_F(ExecutorTest, local_task_steal_after_wakeup) {
  ThreadPoolExecutor executor;
  executor.set_worker_number(2);
  executor.set_local_capacity(1);
  executor.set_enable_work_stealing(true);
  executor.start();
  ::std::this_thread::sleep_for(::std::chrono::milliseconds {100});

  executor
      .execute([&] {
        auto future = executor.execute([] {});
        ASSERT_FALSE(future.wait_for(::std::chrono::milliseconds {100}));
        executor.wakeup_one_worker();
        future.get();
      })
      .get();
}

TEST_F(ExecutorTest, local_task_auto_balance) {
  ThreadPoolExecutor executor;
  executor.set_worker_number(2);
  executor.set_local_capacity(1);
  executor.set_balance_interval(::std::chrono::milliseconds {1});
  executor.start();
  ::std::this_thread::sleep_for(::std::chrono::milliseconds {100});

  executor
      .execute([&] {
        executor.execute([] {}).get();
      })
      .get();
}

TEST_F(ExecutorTest, press) {
  ThreadPoolExecutor executor;
  executor.set_worker_number(64);
  executor.set_global_capacity(128);
  executor.start();

  size_t concurrent = 32;
  size_t times = 2000;

  ::std::vector<Future<size_t>> level1_futures;
  ::std::vector<Future<size_t>> level2_futures;
  level1_futures.resize(concurrent);
  level2_futures.resize(concurrent * times);
  for (size_t i = 0; i < concurrent; ++i) {
    level1_futures[i] = executor.execute([&, i] {
      size_t sum = 0;
      for (size_t j = 0; j < times; ++j) {
        ::std::string s(i * times + j, 'x');
        level2_futures[i * times + j] = executor.execute(
            [](::std::string&& s) {
              return s.size();
            },
            ::std::move(s));
        sum += i * times + j;
      }
      return sum;
    });
  }

  size_t expect_sum = 0;
  for (auto& future : level1_futures) {
    expect_sum += future.get();
  }
  size_t get_sum = 0;
  for (auto& future : level2_futures) {
    get_sum += future.get();
  }
  ASSERT_EQ(expect_sum, get_sum);
}
