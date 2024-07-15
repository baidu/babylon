#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct ListTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

TEST_F(ListTest, support_varint_element) {
  using S = ::std::list<int>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s(gen() % 10, gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(ListTest, support_fixed_element) {
  using S = ::std::list<float>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s(gen() % 10, gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(ListTest, empty_serialize_to_nothing) {
  using S = ::std::list<::std::string>;
  S s;
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  ASSERT_TRUE(string.empty());
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_TRUE(ss.empty());
}

struct SimpleListSerializable {
  SimpleListSerializable() = default;
  SimpleListSerializable(::std::mt19937_64& gen) : s(gen() % 10, gen()) {}

  bool operator==(const SimpleListSerializable& o) const {
    return s == o.s;
  }

  ::std::list<int> s;

  BABYLON_SERIALIZABLE((s, 1))
};
struct ComplexListSerializable {
  ComplexListSerializable() = default;
  ComplexListSerializable(::std::mt19937_64& gen) : s(gen() % 10, gen) {}

  bool operator==(const ComplexListSerializable& o) const {
    return s == o.s;
  }

  ::std::list<SimpleListSerializable> s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(ListTest, support_cascading) {
  using S = ComplexListSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(ListTest, support_print) {
  ComplexListSerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

#endif // BABYLON_USE_PROTOBUF
