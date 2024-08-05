#include "babylon/concurrent/transient_hash_table.h"

#include "gtest/gtest.h"

#include <random>
#include <thread>

using ::babylon::ConcurrentFixedSwissTable;
using ::babylon::ConcurrentTransientHashMap;
using ::babylon::ConcurrentTransientHashSet;

TEST(fixed_swiss_table, default_constructible) {
  ConcurrentFixedSwissTable<::std::string> table;
  ASSERT_LT(0, table.bucket_count());
  ASSERT_EQ(0, table.size());
  ASSERT_TRUE(table.empty());
}

TEST(fixed_swiss_table, default_constructed_table_full_and_also_empty) {
  ConcurrentFixedSwissTable<::std::string> table;
  ASSERT_EQ(0, table.size());
  ASSERT_TRUE(table.empty());
  auto result = table.emplace("10086");
  ASSERT_EQ(table.end(), result.first);
  ASSERT_FALSE(result.second);
  ASSERT_EQ(0, table.size());
  ASSERT_TRUE(table.empty());
}

TEST(fixed_swiss_table, default_constructed_table_usable_after_clear) {
  ConcurrentFixedSwissTable<::std::string> table;
  table.clear();
  auto result = table.emplace("10086");
  ASSERT_NE(table.end(), result.first);
  ASSERT_TRUE(result.second);
  ASSERT_EQ(1, table.size());
  ASSERT_FALSE(table.empty());
}

TEST(fixed_swiss_table, default_constructed_table_usable_after_reserve) {
  ConcurrentFixedSwissTable<::std::string> table;
  table.reserve(16);
  auto result = table.emplace("10086");
  ASSERT_NE(table.end(), result.first);
  ASSERT_TRUE(result.second);
  ASSERT_EQ(1, table.size());
  ASSERT_FALSE(table.empty());
}

TEST(fixed_swiss_table, direct_usable_if_construct_with_bucket_count) {
  ConcurrentFixedSwissTable<::std::string> table {
      ConcurrentFixedSwissTable<::std::string>().bucket_count()};
  auto result = table.emplace("10086");
  ASSERT_NE(table.end(), result.first);
  ASSERT_TRUE(result.second);
  ASSERT_EQ(1, table.size());
  ASSERT_FALSE(table.empty());
}

TEST(fixed_swiss_table, move_constructible) {
  ConcurrentFixedSwissTable<::std::string> table {128};
  table.emplace("10086");
  ConcurrentFixedSwissTable<::std::string> moved_table {::std::move(table)};
  ASSERT_EQ(0, table.size());
  ASSERT_TRUE(table.empty());
  ASSERT_FALSE(table.contains("10086"));
  ASSERT_EQ(1, moved_table.size());
  ASSERT_FALSE(moved_table.empty());
  ASSERT_TRUE(moved_table.contains("10086"));
}

TEST(fixed_swiss_table, copy_constructible) {
  ConcurrentFixedSwissTable<::std::string> table {128};
  table.emplace("10086");
  ConcurrentFixedSwissTable<::std::string> copyed_table {table};
  ASSERT_EQ(1, table.size());
  ASSERT_FALSE(table.empty());
  ASSERT_TRUE(table.contains("10086"));
  ASSERT_EQ(1, copyed_table.size());
  ASSERT_FALSE(copyed_table.empty());
  ASSERT_TRUE(copyed_table.contains("10086"));
}

TEST(fixed_swiss_table, emplace_fail_after_full) {
  ConcurrentFixedSwissTable<::std::string> table {32};
  for (size_t i = 0; i < table.bucket_count(); ++i) {
    auto result = table.emplace(::std::to_string(i));
    ASSERT_NE(table.end(), result.first);
    ASSERT_TRUE(result.second);
  }
  ASSERT_EQ(table.bucket_count(), table.size());
  auto result = table.emplace("10086");
  ASSERT_EQ(table.end(), result.first);
  ASSERT_FALSE(result.second);
  ASSERT_EQ(table.bucket_count(), table.size());
}

TEST(fixed_swiss_table, clear_make_full_table_usable_again) {
  ConcurrentFixedSwissTable<::std::string> table {32};
  for (size_t i = 0; i < table.bucket_count(); ++i) {
    auto result = table.emplace(::std::to_string(i));
    ASSERT_NE(table.end(), result.first);
    ASSERT_TRUE(result.second);
    ASSERT_NE(i, table.size());
  }
  auto result = table.emplace("10086");
  ASSERT_EQ(table.end(), result.first);
  ASSERT_FALSE(result.second);
  ASSERT_EQ(table.bucket_count(), table.size());
  ASSERT_FALSE(table.empty());
  table.clear();
  ASSERT_EQ(0, table.size());
  ASSERT_TRUE(table.empty());
  for (size_t i = 0; i < table.bucket_count(); ++i) {
    auto result = table.emplace(::std::to_string(i + 10086));
    ASSERT_NE(table.end(), result.first);
    ASSERT_TRUE(result.second);
    ASSERT_NE(i, table.size());
  }
}

TEST(fixed_swiss_table, emplace_only_once) {
  ConcurrentFixedSwissTable<::std::string> table {16};
  auto result = table.emplace("10086");
  ASSERT_NE(table.end(), result.first);
  ASSERT_TRUE(result.second);
  auto conflict_result = table.emplace("10086");
  ASSERT_EQ(conflict_result.first, result.first);
  ASSERT_EQ(*conflict_result.first, *result.first);
  ASSERT_EQ(&*conflict_result.first, &*result.first);
  ASSERT_FALSE(conflict_result.second);
  ASSERT_EQ(1, table.size());
}

TEST(fixed_swiss_table, find_item_insert_before) {
  ConcurrentFixedSwissTable<::std::string> table {16};
  ASSERT_EQ(0, table.count("10086"));
  ASSERT_EQ(table.end(), table.find("10086"));
  table.emplace("10086");
  ASSERT_NE(table.end(), table.find("10086"));
  ASSERT_EQ("10086", *table.find("10086"));
  ASSERT_EQ(1, table.count("10086"));
}

TEST(fixed_swiss_table, empty_table_iterable_but_get_nothing) {
  {
    ConcurrentFixedSwissTable<::std::string> table;
    for (auto& value : table) {
      (void)value;
      ASSERT_FALSE(true);
    }
  }
  {
    ConcurrentFixedSwissTable<::std::string> table {32};
    for (auto& value : table) {
      (void)value;
      ASSERT_FALSE(true);
    }
  }
}

TEST(fixed_swiss_table, support_normal_emplace) {
  struct S {
    S(const ::std::string& os) : s(os) {}
    bool operator==(const ::std::string& os) const {
      return s == os;
    }
    ::std::string s;
  };
  struct HS {
    size_t operator()(const ::std::string& s) {
      return ::std::hash<::std::string>()(s);
    }
  };
  ConcurrentFixedSwissTable<S, HS> table {16};
  auto result = table.emplace("10086");
  ASSERT_NE(table.end(), result.first);
  ASSERT_TRUE(result.second);
}

TEST(fixed_swiss_table, iterable) {
  ConcurrentFixedSwissTable<::std::string> table {128};
  ::std::mt19937 gen(::std::random_device {}());
  size_t expected = 0;
  for (size_t i = 0; i < 64; ++i) {
    auto value = static_cast<uint32_t>(gen());
    expected += value;
    table.emplace(::std::to_string(value));
  }
  size_t sum = 0;
  for (auto& value : table) {
    sum += ::std::stoul(value);
  }
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(expected, sum);
  auto& const_table =
      static_cast<const ConcurrentFixedSwissTable<::std::string>&>(table);
  sum = 0;
  for (auto& value : const_table) {
    sum += ::std::stoul(value);
  }
  ASSERT_EQ(expected, sum);
}

TEST(fixed_swiss_table, reserve_keep_items) {
  ConcurrentFixedSwissTable<::std::string> table {256};
  ::std::mt19937 gen(::std::random_device {}());
  size_t expected = 0;
  for (size_t i = 0; i < 64; ++i) {
    auto value = static_cast<uint32_t>(gen());
    expected += value;
    table.emplace(::std::to_string(value));
  }
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(256, table.bucket_count());
  table.reserve(64);
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(256, table.bucket_count());
  table.reserve(128);
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(256, table.bucket_count());
  table.reserve(512);
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(512, table.bucket_count());
  size_t sum = 0;
  for (auto& value : table) {
    sum += ::std::stoul(value);
  }
  ASSERT_EQ(expected, sum);
}

TEST(fixed_swiss_table, rehash_can_shrink) {
  ConcurrentFixedSwissTable<::std::string> table {256};
  ::std::mt19937 gen(::std::random_device {}());
  size_t expected = 0;
  for (size_t i = 0; i < 64; ++i) {
    auto value = static_cast<uint32_t>(gen());
    expected += value;
    table.emplace(::std::to_string(value));
  }
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(256, table.bucket_count());
  {
    size_t sum = 0;
    for (auto& value : table) {
      sum += ::std::stoul(value);
    }
    ASSERT_EQ(expected, sum);
  }
  table.rehash(512);
  {
    size_t sum = 0;
    for (auto& value : table) {
      sum += ::std::stoul(value);
    }
    ASSERT_EQ(expected, sum);
  }
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(512, table.bucket_count());
  table.rehash(128);
  {
    size_t sum = 0;
    for (auto& value : table) {
      sum += ::std::stoul(value);
    }
    ASSERT_EQ(expected, sum);
  }
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(128, table.bucket_count());
  table.rehash(32);
  {
    size_t sum = 0;
    for (auto& value : table) {
      sum += ::std::stoul(value);
    }
    ASSERT_EQ(expected, sum);
  }
  ASSERT_EQ(64, table.size());
  ASSERT_EQ(64, table.bucket_count());
  size_t sum = 0;
  for (auto& value : table) {
    sum += ::std::stoul(value);
  }
  ASSERT_EQ(expected, sum);
}

TEST(hash_set, default_constructible) {
  ConcurrentTransientHashSet<::std::string> set;
  ASSERT_LT(0, set.bucket_count());
  ASSERT_EQ(0, set.size());
  ASSERT_TRUE(set.empty());
}

TEST(hash_set, constructible_with_bucket_count) {
  ConcurrentTransientHashSet<::std::string> set {4096};
  ASSERT_EQ(4096, set.bucket_count());
  ASSERT_EQ(0, set.size());
  ASSERT_TRUE(set.empty());
}

TEST(hash_set, bucket_count_ceil_to_pow2) {
  auto min_bucket_count =
      ConcurrentTransientHashSet<::std::string> {}.bucket_count();
  for (size_t i = 0; i <= min_bucket_count; ++i) {
    ASSERT_EQ(min_bucket_count,
              ConcurrentTransientHashSet<::std::string> {i}.bucket_count());
  }
  for (size_t i = min_bucket_count + 1; i <= 2 * min_bucket_count; ++i) {
    ASSERT_EQ(2 * min_bucket_count,
              ConcurrentTransientHashSet<::std::string> {i}.bucket_count());
  }
  ASSERT_EQ(4 * min_bucket_count,
            ConcurrentTransientHashSet<::std::string> {2 * min_bucket_count + 1}
                .bucket_count());
}

TEST(hash_set, move_constructible) {
  ConcurrentTransientHashSet<::std::string> set;
  ASSERT_LT(0, set.bucket_count());
}

TEST(hash_set, find_item_insert_before) {
  ConcurrentTransientHashSet<::std::string> set {16};
  ASSERT_EQ(0, set.count("10086"));
  ASSERT_EQ(set.end(), set.find("10086"));
  set.emplace("10086");
  ASSERT_NE(set.end(), set.find("10086"));
  ASSERT_EQ("10086", *set.find("10086"));
  ASSERT_EQ(1, set.count("10086"));
}

TEST(hash_set, iterable) {
  ConcurrentTransientHashSet<::std::string> set {128};
  ::std::mt19937 gen(::std::random_device {}());
  size_t expected = 0;
  for (size_t i = 0; i < 128 + 64; ++i) {
    auto value = static_cast<uint32_t>(gen());
    expected += value;
    set.emplace(::std::to_string(value));
  }
  size_t sum = 0;
  for (auto& value : set) {
    sum += ::std::stoul(value);
  }
  ASSERT_EQ(expected, sum);
  auto& const_set =
      static_cast<const ConcurrentTransientHashSet<::std::string>&>(set);
  sum = 0;
  for (auto& value : const_set) {
    sum += ::std::stoul(value);
  }
  ASSERT_EQ(expected, sum);
}

TEST(fixed_swiss_table, concurrent_emplace_and_find_correct) {
  ConcurrentFixedSwissTable<::std::string> set {128 * 129};
  ::std::vector<::std::thread> threads;
  threads.reserve(128);
  for (size_t i = 0; i < 128; ++i) {
    threads.emplace_back([&, i] {
      for (size_t j = 0; j < 256; ++j) {
        set.find(::std::to_string((i - 1) * 128 + j));
        set.emplace(::std::to_string(i * 128 + j));
      }
    });
  }
  size_t expected = 0;
  for (size_t i = 0; i < 129; ++i) {
    for (size_t j = 0; j < 128; ++j) {
      expected += i * 128 + j;
    }
  }
  for (auto& thread : threads) {
    thread.join();
  }
  size_t sum = 0;
  size_t count = 0;
  for (auto& value : set) {
    sum += ::std::stoul(value);
    ++count;
  }
  ASSERT_EQ(128 * 129, count);
  ASSERT_EQ(expected, sum);
}

TEST(hash_set, concurrent_emplace_and_find_correct) {
  ConcurrentTransientHashSet<::std::string> set {128};
  ::std::vector<::std::thread> threads;
  threads.reserve(128);
  for (size_t i = 0; i < 128; ++i) {
    threads.emplace_back([&, i] {
      for (size_t j = 0; j < 256; ++j) {
        set.find(::std::to_string((i - 1) * 128 + j));
        set.emplace(::std::to_string(i * 128 + j));
      }
    });
  }
  size_t expected = 0;
  for (size_t i = 0; i < 129; ++i) {
    for (size_t j = 0; j < 128; ++j) {
      expected += i * 128 + j;
    }
  }
  for (auto& thread : threads) {
    thread.join();
  }
  size_t sum = 0;
  size_t count = 0;
  for (auto& value : set) {
    sum += ::std::stoul(value);
    ++count;
  }
  ASSERT_EQ(128 * 129, count);
  ASSERT_EQ(expected, sum);
}

TEST(hash_map, default_constructible) {
  ConcurrentTransientHashMap<::std::string, ::std::string> map;
  ASSERT_LT(0, map.bucket_count());
}

TEST(hash_map, constructible_with_bucket_count) {
  ConcurrentTransientHashMap<::std::string, ::std::string> map {4096};
  ASSERT_EQ(4096, map.bucket_count());
}

TEST(hash_map, copy_constructible) {
  ConcurrentTransientHashMap<::std::string, ::std::string> map;
  map.emplace("10086", "10010");
  ConcurrentTransientHashMap<::std::string, ::std::string> copyed_map(map);
  ConcurrentTransientHashMap<::std::string, ::std::string> copy_assigned_map;
  copy_assigned_map.emplace("10010", "10086");
  copy_assigned_map = map;
  ASSERT_TRUE(map.contains("10086"));
  ASSERT_TRUE(copyed_map.contains("10086"));
  ASSERT_TRUE(copy_assigned_map.contains("10086"));
  ASSERT_FALSE(copy_assigned_map.contains("10010"));
}

TEST(hash_map, move_constructible) {
  ConcurrentTransientHashMap<::std::string, ::std::string> map;
  map.emplace("10086", "10010");
  ConcurrentTransientHashMap<::std::string, ::std::string> moved_map(
      ::std::move(map));
  ConcurrentTransientHashMap<::std::string, ::std::string> move_assigned_map;
  move_assigned_map.emplace("10010", "10086");
  move_assigned_map = ::std::move(moved_map);
  ASSERT_FALSE(map.contains("10086"));
  ASSERT_FALSE(moved_map.contains("10086"));
  ASSERT_TRUE(move_assigned_map.contains("10086"));
  ASSERT_FALSE(move_assigned_map.contains("10010"));
}

TEST(hash_map, support_try_emplace) {
  ConcurrentTransientHashMap<::std::string, ::std::string> map;
  auto result = map.try_emplace("10086", "10010");
  ASSERT_TRUE(result.second);
  ASSERT_EQ("10086", result.first->first);
  ASSERT_EQ("10010", result.first->second);
  result = map.try_emplace("10086", "11086");
  ASSERT_FALSE(result.second);
  ASSERT_EQ("10086", result.first->first);
  ASSERT_EQ("10010", result.first->second);
}

TEST(hash_map, support_brackets_operator) {
  ConcurrentTransientHashMap<::std::string, ::std::string> map;
  auto& value = map["10086"];
  ASSERT_TRUE(value.empty());
  ASSERT_EQ(&value, &map["10086"]);
  value = "10010";
  ASSERT_EQ("10010", map["10086"]);
  ASSERT_EQ(&value, &map["10086"]);
}

TEST(hash_map, support_non_copyable_nor_moveable_emplace) {
  struct S {
    S() = default;
    S(S&&) = delete;
    S(const S&) = delete;
    S& operator=(S&&) = delete;
    S& operator=(const S&) = delete;

    int v {10010};
  };
  ConcurrentTransientHashMap<::std::string, S> map;
  ASSERT_EQ(10010, map["10086"].v);
  map["10086"].v = 1024;
  ASSERT_EQ(1024, map["10086"].v);
}

TEST(hash_map, support_non_copyable_when_reserve) {
  struct S {
    // S() = default;
    S(S&&) = default;
    S(const S&) = delete;
    // S& operator=(S&&) = default;
    S& operator=(const S&) = delete;
  };
  ConcurrentTransientHashMap<::std::string, S> map;
  map.reserve(1024);
  map.rehash(4096);
}

TEST(hash_map, support_non_copyable_in_vector) {
  struct S {
    // S() = default;
    // S(S&&) = default;
    S(const S&) = delete;
    // S& operator=(S&&) = default;
    S& operator=(const S&) = delete;
  };
  ::std::vector<ConcurrentTransientHashMap<::std::string, S>> vector;
  vector.emplace_back(100);
  vector.emplace_back(200);
}
