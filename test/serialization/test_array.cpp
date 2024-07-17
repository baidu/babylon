#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include <gtest/gtest.h>

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct ArrayTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};

  template <typename T, size_t N>
  void assert_eq(T (&left)[N], T (&right)[N]) {
    for (size_t i = 0; i < N; ++i) {
      ASSERT_EQ(left[i], right[i]);
    }
  }
};

template <typename T, size_t N>
bool eq(T (&left)[N], T (&right)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (!(left[i] == right[i])) {
      return false;
    }
  }
  return true;
}

TEST_F(ArrayTest, serializable_with_salar_element) {
  ASSERT_TRUE(SerializeTraits<int[4]>::SERIALIZABLE);
  int a[4] = {10010, 10086, 0, 0};
  ASSERT_TRUE(Serialization::serialize_to_string(a, string));
  int aa[4] = {-1, -1, -1, -1};
  ASSERT_TRUE(Serialization::parse_from_string(string, aa));
  assert_eq(a, aa);
}

TEST_F(ArrayTest, support_varint_element) {
  using S = int[4];
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  for (auto& v : s) {
    v = gen();
  }
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  assert_eq(s, ss);
}

TEST_F(ArrayTest, support_fixed_element) {
  using S = float[4];
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  for (auto& v : s) {
    v = gen();
  }
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  assert_eq(s, ss);
}

struct SimpleArraySerializable {
  SimpleArraySerializable() = default;
  SimpleArraySerializable(::std::mt19937_64& gen) {
    for (auto& v : s) {
      v = gen();
    }
  }

  bool operator==(const SimpleArraySerializable& o) const {
    return eq(s, o.s);
  }

  int s[4];

  BABYLON_SERIALIZABLE((s, 1))
};
struct ComplexArraySerializable {
  ComplexArraySerializable() = default;
  ComplexArraySerializable(::std::mt19937_64& gen) : s {gen, gen, gen, gen} {}

  bool operator==(const ComplexArraySerializable& o) const {
    return eq(s, o.s);
  }

  SimpleArraySerializable s[4];

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(ArrayTest, support_cascading) {
  using S = ComplexArraySerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(ArrayTest, support_print) {
  ComplexArraySerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

#endif // BABYLON_USE_PROTOBUF
