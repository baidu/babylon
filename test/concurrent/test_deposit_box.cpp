#include "babylon/concurrent/deposit_box.h"

#include "gtest/gtest.h"

TEST(deposit_box, take_only_once) {
  auto& box = ::babylon::DepositBox<int>::instance();
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
  auto& box = ::babylon::DepositBox<int>::instance();
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
