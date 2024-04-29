#include <babylon/executor.h>

#include <gtest/gtest.h>

#include <future>

using ::babylon::AlwaysUseNewThreadExecutor;
using ::babylon::async;
using ::babylon::Executor;
using ::babylon::Future;
using ::babylon::InplaceExecutor;
using ::babylon::MoveOnlyFunction;
using ::babylon::ThreadPoolExecutor;

static InplaceExecutor& inplace_executor = InplaceExecutor::instance();

TEST(executor, can_execute_normal_function) {
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

TEST(executor, can_execute_member_function) {
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

TEST(executor, can_execute_function_object) {
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

TEST(executor, can_execute_lambda) {
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

TEST(executor, can_execute_binded_function) {
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

TEST(executor, handle_void_return) {
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

TEST(executor, handle_non_copyable_function_and_args) {
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

TEST(executor, return_invalid_future_when_invoke_fail) {
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

TEST(inplace_executor, flatten_reentry_execution) {
  InplaceExecutor executor {true};
  int value {1};
  int see[3] {0, 0, 0};
  executor.execute([&] {
    executor.submit([&] {
      executor.execute([&] {
        see[2] = value++;
      });
      see[1] = value++;
    });
    see[0] = value++;
  });
  ASSERT_EQ(1, see[0]);
  ASSERT_EQ(2, see[1]);
  ASSERT_EQ(3, see[2]);
  see[0] = 0;
  see[1] = 0;
  see[2] = 0;
  inplace_executor.execute([&] {
    inplace_executor.submit([&] {
      inplace_executor.execute([&] {
        see[2] = value++;
      });
      see[1] = value++;
    });
    see[0] = value++;
  });
  ASSERT_EQ(6, see[0]);
  ASSERT_EQ(5, see[1]);
  ASSERT_EQ(4, see[2]);
}

TEST(always_use_new_thread_executor, run_async_in_new_thread) {
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

TEST(thread_pool_executor, run_async_in_thread_pool) {
  ThreadPoolExecutor executor;
  ASSERT_EQ(0, executor.initialize(1, 8));
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

TEST(thread_pool_executor, press) {
  ThreadPoolExecutor executor;
  ASSERT_EQ(0, executor.initialize(64, 128));

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
