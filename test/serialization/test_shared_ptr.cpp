#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct SharedPtrTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

struct NonSerializable {};
TEST_F(SharedPtrTest, serializable_same_as_instance) {
  ASSERT_FALSE(
      SerializeTraits<::std::shared_ptr<NonSerializable>>::SERIALIZABLE);
  {
    using T = int;
    using S = ::std::shared_ptr<T>;
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
    using S = ::std::shared_ptr<T>;
    ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
    S s {new T(::std::to_string(gen()))};
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    S ss;
    ASSERT_TRUE(Serialization::parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(*s, *ss);
  }
}

TEST_F(SharedPtrTest, support_const_type) {
  {
    using T = int;
    using S = ::std::shared_ptr<const T>;
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
    using S = ::std::shared_ptr<const T>;
    ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
    S s {new T(::std::to_string(gen()))};
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    S ss;
    ASSERT_TRUE(Serialization::parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(*s, *ss);
  }
}

TEST_F(SharedPtrTest, always_deserialize_to_new_instance) {
  using T = ::std::string;
  using S = ::std::shared_ptr<T>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {new T(::std::to_string(gen()))};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss {new T};
  auto pss = &*ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_TRUE(ss);
  ASSERT_EQ(*s, *ss);
  ASSERT_NE(pss, &*ss);
}

TEST_F(SharedPtrTest, empty_serialize_to_nothing) {
  using T = ::std::string;
  using S = ::std::shared_ptr<T>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  ASSERT_TRUE(string.empty());
  ASSERT_TRUE(Serialization::parse_from_string(string, s));
  ASSERT_FALSE(s);
}

struct SimpleSharedPtrSerializable {
  SimpleSharedPtrSerializable() = default;
  SimpleSharedPtrSerializable(::std::mt19937_64& gen) : s {new int(gen())} {}

  bool operator==(const SimpleSharedPtrSerializable& o) const {
    return (!s && !o.s) || (s && o.s && *s == *o.s);
  }

  ::std::shared_ptr<int> s;

  BABYLON_SERIALIZABLE((s, 1))
};
struct ComplexSharedPtrSerializable {
  ComplexSharedPtrSerializable() = default;
  ComplexSharedPtrSerializable(::std::mt19937_64& gen)
      : s {new SimpleSharedPtrSerializable {gen}} {}

  bool operator==(const ComplexSharedPtrSerializable& o) const {
    return (!s && !o.s) || (s && o.s && *s == *o.s);
  }

  ::std::shared_ptr<SimpleSharedPtrSerializable> s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(SharedPtrTest, support_cascading) {
  using S = ComplexSharedPtrSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(SharedPtrTest, support_print) {
  ComplexSharedPtrSerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

#endif // BABYLON_USE_PROTOBUF
