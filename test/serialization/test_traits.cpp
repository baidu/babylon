#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::CodedOutputStream;

struct SerializeTraitsTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

TEST_F(SerializeTraitsTest, default_not_serializable) {
  struct S {};
  ASSERT_FALSE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  ASSERT_FALSE(Serialization::serialize_to_string(s, string));
  ASSERT_FALSE(Serialization::parse_from_string(string, s));
}

struct MinimalSerializable {
  MinimalSerializable() = default;
  MinimalSerializable(::std::mt19937_64& gen) : v(gen()) {}
  void serialize(CodedOutputStream& os) const {
    SerializeTraits<int>::serialize(v, os);
  }
  bool deserialize(CodedInputStream& is) {
    return SerializeTraits<int>::deserialize(is, v);
  }
  size_t calculate_serialized_size() const {
    return SerializeTraits<int>::calculate_serialized_size(v);
  }
  int v {0};
};
TEST_F(SerializeTraitsTest, minimal_serialize_protocol) {
  using S = MinimalSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss {gen};
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.v, ss.v);
}

TEST_F(SerializeTraitsTest, can_serialize_to_array) {
  using S = MinimalSerializable;
  S s {gen};
  auto size = Serialization::calculate_serialized_size(s);
  char array[size];
  ASSERT_TRUE(
      Serialization::serialize_to_array_with_cached_size(s, array, size));
  S ss {gen};
  ASSERT_TRUE(Serialization::parse_from_array(array, size, ss));
  ASSERT_EQ(s.v, ss.v);
}

TEST_F(SerializeTraitsTest,
       calculate_size_before_serialize_when_cache_is_needed) {
  struct S : public MinimalSerializable {
    void serialize(CodedOutputStream& os) const {
      calculate_serialized_size_before_serialize =
          calculate_serialized_size_called;
      MinimalSerializable::serialize(os);
    }
    size_t calculate_serialized_size() const {
      calculate_serialized_size_called = true;
      return MinimalSerializable::calculate_serialized_size();
    }
    mutable bool calculate_serialized_size_called {false};
    mutable bool calculate_serialized_size_before_serialize {false};
  };
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  ASSERT_FALSE(SerializeTraits<S>::SERIALIZED_SIZE_CACHED);

  {
    S s;
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    ASSERT_FALSE(s.calculate_serialized_size_called);
  }

  struct SS : public S {
    size_t serialized_size_cached() const {
      return 0;
    }
  };
  ASSERT_TRUE(SerializeTraits<SS>::SERIALIZABLE);
  ASSERT_TRUE(SerializeTraits<SS>::SERIALIZED_SIZE_CACHED);

  {
    SS s;
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    ASSERT_TRUE(s.calculate_serialized_size_before_serialize);
  }
}

#endif // BABYLON_USE_PROTOBUF
