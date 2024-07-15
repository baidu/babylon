#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct VectorTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

TEST_F(VectorTest, support_varint_element) {
  using S = ::std::vector<int>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s(gen() % 10, gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(VectorTest, support_fixed_element) {
  using S = ::std::vector<float>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s(gen() % 10, gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(VectorTest, empty_serialize_to_nothing) {
  using S = ::std::vector<::std::string>;
  S s;
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  ASSERT_TRUE(string.empty());
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_TRUE(ss.empty());
}

struct SimpleVectorSerializable {
  SimpleVectorSerializable() = default;
  SimpleVectorSerializable(::std::mt19937_64& gen) : s(gen() % 10, gen()) {}

  bool operator==(const SimpleVectorSerializable& o) const {
    return s == o.s;
  }

  ::std::vector<int> s;

  BABYLON_SERIALIZABLE((s, 1))
};
struct ComplexVectorSerializable {
  ComplexVectorSerializable() = default;
  ComplexVectorSerializable(::std::mt19937_64& gen) : s(gen() % 10, gen) {}

  bool operator==(const ComplexVectorSerializable& o) const {
    return s == o.s;
  }

  ::std::vector<SimpleVectorSerializable> s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(VectorTest, support_cascading) {
  using S = ComplexVectorSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(VectorTest, support_print) {
  ComplexVectorSerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

#endif // BABYLON_USE_PROTOBUF
