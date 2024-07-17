#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;

struct SetTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

TEST_F(SetTest, support_varint_element) {
  using S = ::std::unordered_set<int>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  ::std::vector<int> sample(gen() % 10, gen());
  S s(sample.begin(), sample.end());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(SetTest, support_fixed_element) {
  using S = ::std::unordered_set<float>;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  ::std::vector<float> sample(gen() % 10, gen());
  S s(sample.begin(), sample.end());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

struct SimpleSetSerializable {
  SimpleSetSerializable() = default;
  SimpleSetSerializable(::std::mt19937_64& gen) : i(gen()) {
    ::std::vector<float> sample(gen() % 10, gen());
    s.insert(sample.begin(), sample.end());
  }

  bool operator==(const SimpleSetSerializable& o) const {
    return i == o.i && s == o.s;
  }

  int i {0};
  ::std::unordered_set<int> s;

  BABYLON_SERIALIZABLE((i, 1)(s, 2))
};
namespace std {
template <>
struct hash<SimpleSetSerializable> {
  int operator()(const SimpleSetSerializable& s) const noexcept {
    return s.i;
  }
};
} // namespace std
struct ComplexSetSerializable {
  ComplexSetSerializable() = default;
  ComplexSetSerializable(::std::mt19937_64& gen) {
    for (size_t i = 0; i < gen() % 10; ++i) {
      s.emplace(gen);
    }
  }

  bool operator==(const ComplexSetSerializable& o) const {
    return s == o.s;
  }

  ::std::unordered_set<SimpleSetSerializable> s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(SetTest, support_cascading) {
  using S = ComplexSetSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

TEST_F(SetTest, support_print) {
  ComplexSetSerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

#endif // BABYLON_USE_PROTOBUF
