#include <babylon/serialization.h>

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct UniquePtrTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

struct NonSerializable {};
TEST_F(UniquePtrTest, serializable_same_as_instance) {
  ASSERT_FALSE(
      SerializeTraits<::std::unique_ptr<NonSerializable>>::SERIALIZABLE);
  {
    using T = int;
    using S = ::std::unique_ptr<T>;
    ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
    S s {new T(gen())};
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    S ss;
    ASSERT_TRUE(Serialization::parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(*s, *ss);
  }
  {
    using T = ::std::string;
    using S = ::std::unique_ptr<T>;
    ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
    S s {new T(::std::to_string(gen()))};
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    S ss;
    ASSERT_TRUE(Serialization::parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(*s, *ss);
  }
}

TEST_F(UniquePtrTest, support_const_type) {
  {
    using T = int;
    using S = ::std::unique_ptr<const T>;
    ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
    S s {new T(gen())};
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    S ss;
    ASSERT_TRUE(Serialization::parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(*s, *ss);
  }
  {
    using T = ::std::string;
    using S = ::std::unique_ptr<const T>;
    ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
    S s {new T(::std::to_string(gen()))};
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    S ss;
    ASSERT_TRUE(Serialization::parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(*s, *ss);
  }
}

TEST_F(UniquePtrTest, deserialize_reuse_instance) {
  using T = ::std::string;
  using S = ::std::unique_ptr<T>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {new T(::std::to_string(gen()))};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss {new T};
  auto pss = &*ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_TRUE(ss);
  ASSERT_EQ(*s, *ss);
  ASSERT_EQ(pss, &*ss);
}

TEST_F(UniquePtrTest, empty_serialize_to_nothing) {
  using T = ::std::string;
  using S = ::std::unique_ptr<T>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  ASSERT_TRUE(string.empty());
  ASSERT_TRUE(Serialization::parse_from_string(string, s));
  ASSERT_FALSE(s);
}

struct SimpleUniquePtrSerializable {
  SimpleUniquePtrSerializable() = default;
  SimpleUniquePtrSerializable(::std::mt19937_64& gen) : s {new int(gen())} {}

  bool operator==(const SimpleUniquePtrSerializable& o) const {
    return (!s && !o.s) || (s && o.s && *s == *o.s);
  }

  ::std::unique_ptr<int> s;

  BABYLON_SERIALIZABLE((s, 1))
};
struct ComplexUniquePtrSerializable {
  ComplexUniquePtrSerializable() = default;
  ComplexUniquePtrSerializable(::std::mt19937_64& gen)
      : s {new SimpleUniquePtrSerializable {gen}} {}

  bool operator==(const ComplexUniquePtrSerializable& o) const {
    return (!s && !o.s) || (s && o.s && *s == *o.s);
  }

  ::std::unique_ptr<SimpleUniquePtrSerializable> s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(UniquePtrTest, support_cascading) {
  using S = ComplexUniquePtrSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(UniquePtrTest, support_print) {
  ComplexUniquePtrSerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

#endif // BABYLON_USE_PROTOBUF
