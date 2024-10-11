#include "babylon/concurrent/deposit_box.h"

#include "gtest/gtest.h"

#include <thread>

using ::babylon::DepositBox;

TEST(deposit_box, take_only_once) {
  auto& box = DepositBox<int>::instance();
  auto id = box.emplace(10086);
  {
    auto result = box.take(id);
    ASSERT_TRUE(result);
    ASSERT_EQ(10086, *result);
  }
  {
    auto result = box.take(id);
    ASSERT_FALSE(result);
  }
}

TEST(deposit_box, expire_id_take_nothing) {
  auto& box = DepositBox<int>::instance();
  auto id1 = box.emplace(10086);
  auto index = id1.value;
  box.take(id1);
  auto id2 = box.emplace(10086);
  ASSERT_EQ(index, id2.value);
  ASSERT_FALSE(box.take(id1));
  auto result = box.take(id2);
  ASSERT_TRUE(result);
  ASSERT_EQ(10086, *result);
}

TEST(deposit_box, value_not_reused_in_scope) {
  auto& box = DepositBox<int>::instance();
  auto id1 = box.emplace(10086);
  auto index = id1.value;
  auto accessor = box.take(id1);
  auto id2 = box.emplace(10086);
  ASSERT_NE(index, id2.value);
  ASSERT_FALSE(box.take(id1));
  accessor = DepositBox<int>::Accessor {};
  auto id3 = box.emplace(10086);
  ASSERT_EQ(index, id3.value);
}

TEST(deposit_box, explicit_take_and_finish) {
  auto& box = DepositBox<int>::instance();
  auto id1 = box.emplace(10086);
  auto index = id1.value;
  auto value = box.take_released(id1);
  ASSERT_EQ(10086, *value);
  auto id2 = box.emplace(10086);
  ASSERT_NE(index, id2.value);
  ASSERT_FALSE(box.take(id1));
  ASSERT_EQ(nullptr, box.take_released(id1));
  box.finish_released(id1);
  auto id3 = box.emplace(10086);
  ASSERT_EQ(index, id3.value);
}

TEST(deposit_box, concurrent_works_fine) {
  auto& box = DepositBox<::std::string>::instance();

  ::std::vector<::babylon::VersionedValue<uint32_t>> ids;
  ids.resize(100);
  for (size_t i = 0; i < ids.size(); ++i) {
    if (i % 3 == 0) {
      ids[i] = box.emplace('x', 50);
    } else {
      ids[i] = ids[i - 1];
    }
  }

  ::std::vector<::std::thread> threads;
  threads.reserve(ids.size());
  for (size_t i = 0; i < ids.size(); ++i) {
    threads.emplace_back([&, i] {
      box.take(ids[i]);
      ids[i] = box.emplace('y', 100);
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}
