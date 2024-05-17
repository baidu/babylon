#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct StringTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

TEST_F(StringTest, serializable) {
  using S = ::std::string;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s = ::std::to_string(gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(StringTest, empty_serialize_to_nothing) {
  using S = ::std::string;
  S s;
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  ASSERT_TRUE(string.empty());
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_TRUE(ss.empty());
}

struct SimpleStringSerializable {
  ::std::string s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(StringTest, support_cascading) {
  using S = SimpleStringSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  s.s = ::std::to_string(gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.s, ss.s);
}

#endif // BABYLON_USE_PROTOBUF
