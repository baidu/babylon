#include <babylon/concurrent/id_allocator.h>

#include <gtest/gtest.h>

#include <future>
#include <random>
#include <thread>
#include <unordered_set>

using ::babylon::ThreadId;

TEST(thread_id, current_thread_id_is_printable) {
  ::std::cerr << "cid " << ThreadId::current_thread_id() << ::std::endl;
  ::std::thread([] {
    ::std::cerr << "cid " << ThreadId::current_thread_id() << ::std::endl;
  }).join();
}

TEST(thread_id, same_value_and_version_for_same_thread) {
  ::std::thread([] {
    auto version_and_value = ThreadId::current_thread_id().version_and_value;
    auto value = ThreadId::current_thread_id().value;
    auto version = ThreadId::current_thread_id().version;
    ASSERT_EQ(version_and_value,
              ThreadId::current_thread_id().version_and_value);
    ASSERT_EQ(version, ThreadId::current_thread_id().version);
    ASSERT_EQ(value, ThreadId::current_thread_id().value);
  }).join();
}

TEST(thread_id, different_value_for_different_living_thread) {
  auto version_and_value = ThreadId::current_thread_id().version_and_value;
  auto value = ThreadId::current_thread_id().value;
  ::std::thread([&] {
    ASSERT_NE(version_and_value,
              ThreadId::current_thread_id().version_and_value);
    ASSERT_NE(value, ThreadId::current_thread_id().value);
  }).join();
}

TEST(thread_id, value_reusable_with_different_version) {
  uint32_t version_and_value;
  uint16_t value;
  uint16_t version;
  ::std::thread([&] {
    version_and_value = ThreadId::current_thread_id().version_and_value;
    value = ThreadId::current_thread_id().value;
    version = ThreadId::current_thread_id().version;
  }).join();
  ::std::thread([&] {
    ASSERT_NE(version_and_value,
              ThreadId::current_thread_id().version_and_value);
    ASSERT_NE(version, ThreadId::current_thread_id().version);
    ASSERT_EQ(value, ThreadId::current_thread_id().value);
  }).join();
}

TEST(thread_id, can_iterator_over_alive_threads) {
  size_t thread_num = 128;
  ::std::vector<::std::promise<void>> exit_promises;
  exit_promises.resize(thread_num);
  ::std::vector<uint16_t> ids;
  ids.resize(thread_num);
  ::std::vector<::std::thread> threads;
  threads.reserve(thread_num);
  ::std::atomic<size_t> ready_num = {0};
  // 启动n个线程，记录拿到的ThreadId
  for (size_t i = 0; i < thread_num; ++i) {
    threads.emplace_back([&, i] {
      ids[i] = ThreadId::current_thread_id().value;
      ready_num.fetch_add(1);
      exit_promises[i].get_future().get();
    });
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
  // 等ThreadId都获取完
  while (thread_num != ready_num.load()) {
    ::usleep(1000);
  }
  // 检查for_each接口拿到的ThreadId
  ::std::unordered_set<uint16_t> alive_ids;
  ThreadId::for_each([&](uint16_t begin_value, uint16_t end_value) {
    for (uint16_t i = begin_value; i < end_value; ++i) {
      // 不重
      ASSERT_TRUE(alive_ids.emplace(i).second);
    }
  });
  // 用真实活跃的线程记录的ThreadId进行校验
  for (size_t i = 0; i < thread_num; ++i) {
    if (threads[i].joinable()) {
      // 不丢
      ASSERT_EQ(1, alive_ids.erase(ids[i]));
      exit_promises[i].set_value();
      threads[i].join();
    }
  }
}
