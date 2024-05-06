#include <babylon/concurrent/thread_local.h>

#include <gtest/gtest.h>

#include <future>
#include <random>
#include <thread>

using ::babylon::CompactEnumerableThreadLocal;
using ::babylon::EnumerableThreadLocal;

TEST(thread_local_storage, each_live_thread_has_separate_local_storage) {
  EnumerableThreadLocal<::std::string> storage;
  auto* v1 = &storage.local();
  ::std::thread([&] {
    ASSERT_NE(v1, &storage.local());
  }).join();
}

TEST(thread_local_storage, new_thread_may_reuse_storage) {
  EnumerableThreadLocal<::std::string> storage;
  ::std::thread([&] {
    storage.local() = "10086";
  }).join();
  ::std::thread([&] {
    ASSERT_EQ("10086", storage.local());
  }).join();
}

TEST(thread_local_storage, movable) {
  EnumerableThreadLocal<::std::string> storage;
  ::std::thread([&] {
    storage.local() = "10086";
  }).join();
  EnumerableThreadLocal<::std::string> moved_storage {::std::move(storage)};
  ::std::thread([&] {
    ASSERT_EQ("10086", moved_storage.local());
    ASSERT_NE("10086", storage.local());
  }).join();
  EnumerableThreadLocal<::std::string> move_assigned_storage {
      ::std::move(moved_storage)};
  ::std::thread([&] {
    ASSERT_EQ("10086", move_assigned_storage.local());
    ASSERT_NE("10086", moved_storage.local());
  }).join();
}

TEST(thread_local_storage, can_iterate_over_alive_storage) {
  EnumerableThreadLocal<::std::string> storage;
  size_t thread_num = 128;
  ::std::vector<::std::promise<void>> exit_promises;
  exit_promises.resize(thread_num);
  ::std::vector<::std::thread> threads;
  threads.reserve(thread_num);
  ::std::atomic<size_t> ready_num = {0};
  for (size_t i = 0; i < thread_num; ++i) {
    threads.emplace_back([&, i] {
      storage.local() = ::std::to_string(i);
      ready_num.fetch_add(1);
      exit_promises[i].get_future().get();
    });
  }
  while (thread_num != ready_num.load()) {
    ::usleep(100);
  }
  // 随机停掉大约一半
  ::std::mt19937 gen(::std::random_device {}());
  for (size_t i = 0; i < thread_num / 2; ++i) {
    auto index = gen() % thread_num;
    if (threads[index].joinable()) {
      exit_promises[index].set_value();
      threads[index].join();
    }
  }
  size_t sum = 0;
  storage.for_each([&](::std::string* iter, ::std::string* end) {
    for (; iter != end; ++iter) {
      if (!iter->empty()) {
        sum += ::std::stoull(*iter);
      }
    }
  });
  size_t const_sum = 0;
  const EnumerableThreadLocal<::std::string>& const_storage = storage;
  const_storage.for_each(
      [&](const ::std::string* iter, const ::std::string* end) {
        for (; iter != end; ++iter) {
          if (!iter->empty()) {
            const_sum += ::std::stoull(*iter);
          }
        }
      });
  ASSERT_EQ(sum, const_sum);
  size_t alive_sum = 0;
  storage.for_each_alive([&](::std::string* iter, ::std::string* end) {
    for (; iter != end; ++iter) {
      if (!iter->empty()) {
        alive_sum += ::std::stoull(*iter);
      }
    }
  });
  size_t const_alive_sum = 0;
  const_storage.for_each_alive(
      [&](const ::std::string* iter, const ::std::string* end) {
        for (; iter != end; ++iter) {
          if (!iter->empty()) {
            const_alive_sum += ::std::stoull(*iter);
          }
        }
      });
  ASSERT_EQ(alive_sum, const_alive_sum);
  for (size_t i = 0; i < thread_num; ++i) {
    if (threads[i].joinable()) {
      alive_sum -= i;
      exit_promises[i].set_value();
      threads[i].join();
    }
  }
  ASSERT_EQ((thread_num - 1) * thread_num / 2, sum);
  ASSERT_EQ(0, alive_sum);
}

TEST(thread_local_storage, compact_enumerable_may_adjcent_to_each_other) {
  CompactEnumerableThreadLocal<size_t> compact_storage[8];
  for (size_t i = 0; i < 8; ++i) {
    ASSERT_EQ(&compact_storage[0].local() + i, &compact_storage[i].local());
  }
}

TEST(thread_local_storage, compact_movable) {
  CompactEnumerableThreadLocal<size_t> storage;
  ::std::thread([&] {
    storage.local() = 10086;
  }).join();
  CompactEnumerableThreadLocal<size_t> moved_storage {::std::move(storage)};
  ::std::thread([&] {
    ASSERT_EQ(10086, moved_storage.local());
    ASSERT_NE(10086, storage.local());
  }).join();
  CompactEnumerableThreadLocal<size_t> move_assigned_storage {
      ::std::move(moved_storage)};
  ::std::thread([&] {
    ASSERT_EQ(10086, move_assigned_storage.local());
    ASSERT_NE(10086, moved_storage.local());
  }).join();
}

TEST(thread_local_storage, each_compact_enumerable_is_independent) {
  CompactEnumerableThreadLocal<::std::string> compact_storage[2];
  const CompactEnumerableThreadLocal<::std::string>* ccompact_storage =
      compact_storage;
  size_t thread_num = 128;
  ::std::vector<::std::promise<void>> exit_promises;
  exit_promises.resize(thread_num);
  ::std::vector<::std::thread> threads;
  threads.reserve(thread_num);
  ::std::atomic<size_t> ready_num = {0};
  for (size_t i = 0; i < thread_num; ++i) {
    threads.emplace_back([&, i] {
      compact_storage[0].local() = ::std::to_string(i);
      compact_storage[1].local() = ::std::to_string(i * 10);
      ready_num.fetch_add(1);
      exit_promises[i].get_future().get();
    });
  }
  while (thread_num != ready_num.load()) {
    ::usleep(100);
  }
  ::std::mt19937 gen(::std::random_device {}());
  for (size_t i = 0; i < thread_num / 2; ++i) {
    auto index = gen() % thread_num;
    if (threads[index].joinable()) {
      exit_promises[index].set_value();
      threads[index].join();
    }
  }
  size_t sum[2] = {0, 0};
  compact_storage[0].for_each([&](::std::string& value) {
    if (!value.empty()) {
      sum[0] += ::std::stoull(value);
    }
  });
  ccompact_storage[1].for_each([&](const ::std::string& value) {
    if (!value.empty()) {
      sum[1] += ::std::stoull(value);
    }
  });
  size_t alive_sum[2] = {0, 0};
  compact_storage[0].for_each_alive([&](::std::string& value) {
    if (!value.empty()) {
      alive_sum[0] += ::std::stoull(value);
    }
  });
  ccompact_storage[1].for_each_alive([&](const ::std::string& value) {
    if (!value.empty()) {
      alive_sum[1] += ::std::stoull(value);
    }
  });
  ASSERT_EQ((thread_num - 1) * thread_num / 2, sum[0]);
  ASSERT_EQ((thread_num - 1) * thread_num * 5, sum[1]);
  for (size_t i = 0; i < thread_num; ++i) {
    if (threads[i].joinable()) {
      alive_sum[0] -= i;
      alive_sum[1] -= i * 10;
      exit_promises[i].set_value();
      threads[i].join();
    }
  }
  ASSERT_EQ(0, alive_sum[0]);
  ASSERT_EQ(0, alive_sum[1]);
}
