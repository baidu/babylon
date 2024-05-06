#include "babylon/concurrent/vector.h"

#include <gtest/gtest.h>

#include <thread>

using ::babylon::ConcurrentVector;

TEST(concurrent_vector, construct_with_block_size) {
  { ConcurrentVector<::std::string> vector; }
  { ConcurrentVector<::std::string> vector(128); }
  { ConcurrentVector<::std::string, 1024> vector; }
  { ConcurrentVector<::std::string, 1024> vector(128); }
}

TEST(concurrent_vector, movable) {
  ConcurrentVector<::std::string> vector;
  vector.ensure(2) = "10086";
  ASSERT_EQ("10086", vector[2]);
  ConcurrentVector<::std::string> moved_vector(::std::move(vector));
  ConcurrentVector<::std::string> move_assigned_vector;
  move_assigned_vector = ::std::move(moved_vector);
  ASSERT_EQ("10086", move_assigned_vector[2]);
}

TEST(concurrent_vector, block_size_round_up) {
  for (size_t i = 17; i < 32; ++i) {
    ConcurrentVector<::std::string> vector(i);
    ASSERT_EQ(32, vector.block_size());
  }
}

TEST(concurrent_vector, fix_block_size_at_compile_time) {
  for (size_t i = 0; i < 32; ++i) {
    ConcurrentVector<::std::string, 1024> vector(i);
    ASSERT_EQ(1024, vector.block_size());
  }
}

TEST(concurrent_vector, init_without_first_block) {
  {
    ConcurrentVector<::std::string> vector(128);
    ASSERT_EQ(0, vector.size());
    ASSERT_EQ(128, vector.block_size());
  }
  {
    ConcurrentVector<::std::string, 64> vector;
    ASSERT_EQ(0, vector.size());
    ASSERT_EQ(64, vector.block_size());
  }
}

TEST(concurrent_vector, reserve_extend_size) {
  {
    ConcurrentVector<::std::string> vector(128);
    ASSERT_EQ(0, vector.size());
    vector.reserve(128);
    ASSERT_EQ(128, vector.size());
    vector.reserve(129);
    ASSERT_EQ(256, vector.size());
  }
  {
    ConcurrentVector<::std::string, 64> vector;
    ASSERT_EQ(0, vector.size());
    vector.reserve(128);
    ASSERT_EQ(128, vector.size());
    vector.reserve(129);
    ASSERT_EQ(192, vector.size());
  }
}

TEST(concurrent_vector, ensure_index_valid_and_return_that_object) {
  ConcurrentVector<::std::string> vector(128);
  vector.ensure(0) = "begin";
  vector.ensure(127) = "end";
  ASSERT_EQ(128, vector.size());
  ASSERT_STREQ("begin", vector[0].data());
  ASSERT_STREQ("end", vector[127].data());
  vector.ensure(128) = "new end";
  ASSERT_EQ(256, vector.size());
  ASSERT_STREQ("new end", vector[128].data());
}

TEST(concurrent_vector, bracket_get_object_with_valid_index) {
  ConcurrentVector<::std::string> vector(128);
  vector.reserve(128);
  vector[25] = "10086";
  {
    const auto& cvector = vector;
    ASSERT_STREQ("10086", cvector[25].data());
  }
}

TEST(concurrent_vector, object_pointer_keep_valid_after_extend) {
  ConcurrentVector<::std::string> vector(128);
  vector.reserve(128);
  vector[25] = "10086";
  auto* address = &vector[25];
  ASSERT_EQ(128, vector.size());
  vector.ensure(128) = "new end";
  ASSERT_EQ(256, vector.size());
  ASSERT_EQ(address, &vector[25]);
  ASSERT_STREQ("10086", address->data());
  ASSERT_STREQ("new end", vector[128].data());
}

TEST(concurrent_vector, copy_n_across_block_work_well) {
  ::std::vector<::std::string> data = {"0", "1", "2", "3", "4", "5"};
  ConcurrentVector<::std::string> vector(2);
  ASSERT_EQ(2, vector.block_size());
  vector.copy_n(data.begin() + 1, 4, 1);
  ASSERT_TRUE(vector[0].empty());
  ASSERT_EQ(vector[1], data[1]);
  ASSERT_EQ(vector[2], data[2]);
  ASSERT_EQ(vector[3], data[3]);
  ASSERT_EQ(vector[4], data[4]);
  ASSERT_TRUE(vector[5].empty());
}

TEST(concurrent_vector, copy_n_inside_block_work_well) {
  ::std::vector<::std::string> data = {"0", "1", "2", "3", "4", "5"};
  ConcurrentVector<::std::string> vector(4);
  ASSERT_EQ(4, vector.block_size());
  vector.copy_n(data.begin() + 2, 2, 5);
  ASSERT_TRUE(vector[4].empty());
  ASSERT_EQ(vector[5], "2");
  ASSERT_EQ(vector[6], "3");
  ASSERT_TRUE(vector[7].empty());
}

TEST(concurrent_vector, fill_n_across_block_work_well) {
  ConcurrentVector<::std::string> vector(2);
  vector.reserve(6);
  ASSERT_EQ(2, vector.block_size());
  vector.fill_n(1, 4, "10086");
  ASSERT_TRUE(vector[0].empty());
  ASSERT_EQ(vector[1], "10086");
  ASSERT_EQ(vector[2], "10086");
  ASSERT_EQ(vector[3], "10086");
  ASSERT_EQ(vector[4], "10086");
  ASSERT_TRUE(vector[5].empty());
}

TEST(concurrent_vector, fill_n_inside_block_work_well) {
  ConcurrentVector<::std::string> vector(4);
  vector.reserve(12);
  ASSERT_EQ(4, vector.block_size());
  vector.fill_n(5, 2, "10086");
  ASSERT_TRUE(vector[4].empty());
  ASSERT_EQ(vector[5], "10086");
  ASSERT_EQ(vector[6], "10086");
  ASSERT_TRUE(vector[7].empty());
}

TEST(concurrent_vector, snapshot_not_expend_with_vector) {
  ConcurrentVector<::std::string> vector(128);
  vector.reserve(1);
  ASSERT_EQ(128, vector.size());
  auto snapshot = vector.snapshot();
  vector.ensure(128);
  ASSERT_EQ(256, vector.size());
  ASSERT_EQ(128, snapshot.size());
}

TEST(concurrent_vector, snapshot_valid_after_vector_expend) {
  ConcurrentVector<::std::string> vector(128);
  vector.reserve(1);
  ASSERT_EQ(128, vector.size());
  auto snapshot = vector.snapshot();
  snapshot[10] = "10086";
  vector.ensure(128);
  snapshot[20] = "10010";
  ASSERT_EQ(256, vector.size());
  ASSERT_EQ(128, snapshot.size());
  ASSERT_EQ("10086", snapshot[10]);
  ASSERT_EQ("10010", snapshot[20]);
  ASSERT_EQ("10086", vector[10]);
  ASSERT_EQ("10010", vector[20]);
}

TEST(concurrent_vector, const_snapshot_can_read) {
  ConcurrentVector<::std::string> vector(128);
  vector.reserve(1);
  ASSERT_EQ(128, vector.size());
  vector[10] = "10086";
  {
    const ConcurrentVector<::std::string>& cvector = vector;
    auto csnapshot = cvector.snapshot();
    ASSERT_EQ("10086", csnapshot[10]);
  }
}

TEST(concurrent_vector, reserve_can_get_snapshot_back) {
  ConcurrentVector<::std::string> vector(128);
  vector.reserve(1);
  ASSERT_EQ(128, vector.size());
  auto snapshot = vector.reserved_snapshot(256);
  snapshot[10] = "10086";
  ASSERT_EQ(256, vector.size());
  ASSERT_EQ("10086", vector[10]);
}

TEST(concurrent_vector, support_concurret_access_and_extend) {
  size_t rounds = 20;
  size_t concurrents = 100;
  for (size_t round = 0; round < rounds; ++round) {
    ConcurrentVector<::std::string> vector(1);
    ::std::vector<::std::thread> threads;
    threads.reserve(concurrents);
    for (size_t i = 0; i < concurrents; ++i) {
      threads.emplace_back([i, &vector] {
        auto& v = vector.ensure(i);
        v = ::std::to_string(i);
        vector.gc();
      });
    }
    for (size_t i = 0; i < concurrents; ++i) {
      threads[i].join();
    }
    vector.unsafe_gc();
    ASSERT_EQ(concurrents, vector.size());
    for (size_t i = 0; i < concurrents; ++i) {
      ASSERT_EQ(::std::to_string(i), vector[i]);
    }
  }
}
