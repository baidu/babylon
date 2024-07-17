#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct MapTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

TEST_F(MapTest, support_varint_element) {
  using S = ::std::unordered_map<int, int>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  for (size_t i = 0; i < gen() % 10; ++i) {
    s.emplace(gen(), gen());
  }
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(MapTest, support_fixed_element) {
  using S = ::std::unordered_map<float, float>;
  S s;
  for (size_t i = 0; i < gen() % 10; ++i) {
    s.emplace(gen(), gen());
  }
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

struct SimpleMapSerializable {
  SimpleMapSerializable() = default;
  SimpleMapSerializable(::std::mt19937_64& gen) : i(gen()) {
    for (size_t i = 0; i < gen() % 10; ++i) {
      s.emplace(gen(), gen());
    }
  }

  bool operator==(const SimpleMapSerializable& o) const {
    return i == o.i && s == o.s;
  }

  int i {0};
  ::std::unordered_map<int, int> s;

  BABYLON_SERIALIZABLE((i, 1)(s, 2))
};
namespace std {
template <>
struct hash<SimpleMapSerializable> {
  int operator()(const SimpleMapSerializable& s) const noexcept {
    return s.i;
  }
};
} // namespace std
struct ComplexMapSerializable {
  ComplexMapSerializable() = default;
  ComplexMapSerializable(::std::mt19937_64& gen) {
    for (size_t i = 0; i < gen() % 10; ++i) {
      s.emplace(gen, gen);
    }
  }

  bool operator==(const ComplexMapSerializable& o) const {
    return s == o.s;
  }

  ::std::unordered_map<SimpleMapSerializable, SimpleMapSerializable> s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(MapTest, support_cascading) {
  using S = ComplexMapSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(MapTest, support_print) {
  ComplexMapSerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

#endif // BABYLON_USE_PROTOBUF
