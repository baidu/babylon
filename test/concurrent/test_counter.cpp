#include <babylon/concurrent/counter.h>

#include <gtest/gtest.h>

#include <thread>

using ::babylon::ConcurrentAdder;
using ::babylon::ConcurrentMaxer;
using ::babylon::ConcurrentSummer;

TEST(concurrent_adder, caculate_right) {
  ConcurrentAdder adder;
  ::std::thread a([&] {
    adder << 10;
  });
  ::std::thread b([&] {
    adder << 3;
  });
  ::std::thread c([&] {
    adder << -5;
  });
  a.join();
  b.join();
  c.join();
  ASSERT_EQ(8, adder.value());
}

TEST(concurrent_maxer, caculate_right) {
  ConcurrentMaxer maxer;
  ::std::thread a([&] {
    maxer << 10;
  });
  ::std::thread b([&] {
    maxer << 3;
  });
  ::std::thread c([&] {
    maxer << -5;
  });
  a.join();
  b.join();
  c.join();
  ASSERT_EQ(10, maxer.value());
}

TEST(concurrent_maxer, empty_aware) {
  ConcurrentMaxer maxer;
  ssize_t value = 10086;
  auto has_value = maxer.value(value);
  ASSERT_FALSE(has_value);
  ASSERT_EQ(10086, value);
  ASSERT_EQ(0, maxer.value());

  ::std::thread([&] {
    maxer << 10;
  }).join();

  has_value = maxer.value(value);
  ASSERT_TRUE(has_value);
  ASSERT_EQ(10, value);
  ASSERT_EQ(10, maxer.value());

  maxer.reset();

  value = 10010;
  has_value = maxer.value(value);
  ASSERT_FALSE(has_value);
  ASSERT_EQ(10010, value);
  ASSERT_EQ(0, maxer.value());
}

TEST(concurrent_maxer, resetable) {
  ConcurrentMaxer maxer;
  {
    ::std::thread a([&] {
      maxer << 10;
    });
    ::std::thread b([&] {
      maxer << 3;
    });
    ::std::thread c([&] {
      maxer << -5;
    });
    a.join();
    b.join();
    c.join();
  }
  ASSERT_EQ(10, maxer.value());
  maxer.reset();
  {
    ::std::thread a([&] {
      maxer << 3;
    });
    ::std::thread b([&] {
      maxer << 7;
    });
    ::std::thread c([&] {
      maxer << -2;
    });
    a.join();
    b.join();
    c.join();
  }
  ASSERT_EQ(7, maxer.value());
}

TEST(concurrent_summary, caculate_right) {
  ConcurrentSummer summer;
  ::std::thread a([&] {
    summer << 10;
  });
  ::std::thread b([&] {
    summer << 3;
  });
  ::std::thread c([&] {
    summer.operator<<({-5, 5});
  });
  a.join();
  b.join();
  c.join();
  ASSERT_EQ(8, summer.value().sum);
  ASSERT_EQ(7, summer.value().num);
}
