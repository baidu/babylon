#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializationHelper;
using ::babylon::SerializeTraits;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::CodedOutputStream;

struct AggregateTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};
};

struct BasicTypeSerializable {
  BasicTypeSerializable() = default;
  BasicTypeSerializable(std::mt19937_64& gen)
      : i(gen()), s {::std::to_string(gen())}, v(gen() % 10, gen()) {}

  bool operator==(const BasicTypeSerializable& o) const {
    return i == o.i && s == o.s && v == o.v;
  }

  int i {0};
  ::std::string s;
  ::std::vector<int> v;

  BABYLON_SERIALIZABLE((i, 1)(s, 2)(v, 3))
};
TEST_F(AggregateTest, support_basic_types) {
  using S = BasicTypeSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

struct MemberCascadeSerializable {
  MemberCascadeSerializable() = default;
  MemberCascadeSerializable(::std::mt19937_64& gen) : s {gen} {}

  BasicTypeSerializable s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(AggregateTest, cascade_serializable_as_member) {
  using S = MemberCascadeSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.s, ss.s);
}

struct BaseCascadeSerializable : public BasicTypeSerializable {
  BaseCascadeSerializable() = default;
  BaseCascadeSerializable(::std::mt19937_64& gen)
      : BasicTypeSerializable {gen}, a(gen()) {}

  bool operator==(const BaseCascadeSerializable& o) const {
    return static_cast<const BasicTypeSerializable&>(*this) == o && a == o.a;
  }

  int a {0};

  BABYLON_SERIALIZABLE_WITH_BASE((BasicTypeSerializable, 1), (a, 2))
};
TEST_F(AggregateTest, cascade_serializable_to_base) {
  using S = BaseCascadeSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

struct EmptyMemberSerializable {
  ::std::vector<int> v;

  BABYLON_SERIALIZABLE((v, 1))
};
TEST_F(AggregateTest, empty_field_serialize_to_nothing_and_not_deserialized) {
  using S = EmptyMemberSerializable;
  S s;
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  ASSERT_TRUE(string.empty());
  ASSERT_TRUE(Serialization::parse_from_string(string, s));
  ASSERT_TRUE(s.v.empty());
  s.v.resize(4);
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  ASSERT_FALSE(string.empty());
}

struct ComplexMemberSerializable {
  ::std::vector<::std::string> v;

  BABYLON_SERIALIZABLE((v, 1))
};
TEST_F(AggregateTest, cache_if_member_complex_to_make_it_simple) {
  using S = ComplexMemberSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_CACHED);
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_COMPLEXITY ==
              SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE);

  S s;
  s.v.assign({"some", "string", "value"});
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.v, ss.v);
}

struct ManySimpleMemberSerializable {
  ::std::string s0;
  ::std::string s1;
  ::std::string s2;
  ::std::string s3;
  ::std::string s4;
  ::std::string s5;
  ::std::string s6;
  ::std::string s7;
  ::std::string s8;
  ::std::string s9;

  BABYLON_SERIALIZABLE(
      (s0, 1)(s1, 2)(s2, 3)(s3, 4)(s4, 5)(s5, 6)(s6, 7)(s7, 8)(s8, 9)(s9, 10))
};
TEST_F(AggregateTest, cache_if_too_many_simple_member_to_keep_it_simple) {
  using S = ManySimpleMemberSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_CACHED);
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_COMPLEXITY ==
              SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE);
  ASSERT_GT(sizeof(S), sizeof(::std::string) * 10);

  S s;
  s.s0 = ::std::to_string(gen());
  s.s1 = ::std::to_string(gen());
  s.s2 = ::std::to_string(gen());
  s.s3 = ::std::to_string(gen());
  s.s4 = ::std::to_string(gen());
  s.s5 = ::std::to_string(gen());
  s.s6 = ::std::to_string(gen());
  s.s7 = ::std::to_string(gen());
  s.s8 = ::std::to_string(gen());
  s.s9 = ::std::to_string(gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.s0, ss.s0);
  ASSERT_EQ(s.s1, ss.s1);
  ASSERT_EQ(s.s2, ss.s2);
  ASSERT_EQ(s.s3, ss.s3);
  ASSERT_EQ(s.s4, ss.s4);
  ASSERT_EQ(s.s5, ss.s5);
  ASSERT_EQ(s.s6, ss.s6);
  ASSERT_EQ(s.s7, ss.s7);
  ASSERT_EQ(s.s8, ss.s8);
  ASSERT_EQ(s.s9, ss.s9);
}

struct FewSimpleMemberSerializable {
  ::std::string s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(AggregateTest, few_simple_member_dont_need_cache) {
  using S = FewSimpleMemberSerializable;
  ASSERT_FALSE(SerializeTraits<S>::SERIALIZED_SIZE_CACHED);
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_COMPLEXITY ==
              SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE);
  ASSERT_EQ(sizeof(S), sizeof(::std::string));

  S s;
  s.s = ::std::to_string(gen());
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.s, ss.s);
}

struct FewCacheMemberSerializable {
  ComplexMemberSerializable s;

  BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(AggregateTest, cache_member_dont_need_record_size) {
  using S = FewCacheMemberSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_CACHED);
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_COMPLEXITY ==
              SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE);
  ASSERT_EQ(sizeof(S), sizeof(ComplexMemberSerializable));

  S s;
  s.s.v.assign({"some", "string", "value"});
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.s.v, ss.s.v);
}

struct ManyTrivialMemberSerializable {
  float f0;
  float f1;
  float f2;
  float f3;
  float f4;
  float f5;
  float f6;
  float f7;
  float f8;
  float f9;

  BABYLON_SERIALIZABLE(
      (f0, 1)(f1, 2)(f2, 3)(f3, 4)(f4, 5)(f5, 6)(f6, 7)(f7, 8)(f8, 9)(f9, 10))
};
TEST_F(AggregateTest, trivial_member_dont_need_cache) {
  using S = ManyTrivialMemberSerializable;
  ASSERT_FALSE(SerializeTraits<S>::SERIALIZED_SIZE_CACHED);
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZED_SIZE_COMPLEXITY ==
              SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL);
  ASSERT_EQ(sizeof(S), sizeof(float) * 10);

  S s;
  s.f0 = gen();
  s.f1 = gen();
  s.f2 = gen();
  s.f3 = gen();
  s.f4 = gen();
  s.f5 = gen();
  s.f6 = gen();
  s.f7 = gen();
  s.f8 = gen();
  s.f9 = gen();
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.f0, ss.f0);
  ASSERT_EQ(s.f1, ss.f1);
  ASSERT_EQ(s.f2, ss.f2);
  ASSERT_EQ(s.f3, ss.f3);
  ASSERT_EQ(s.f4, ss.f4);
  ASSERT_EQ(s.f5, ss.f5);
  ASSERT_EQ(s.f6, ss.f6);
  ASSERT_EQ(s.f7, ss.f7);
  ASSERT_EQ(s.f8, ss.f8);
  ASSERT_EQ(s.f9, ss.f9);
}

TEST_F(AggregateTest, print_as_object) {
  ASSERT_TRUE(SerializeTraits<BasicTypeSerializable>::PRINT_AS_OBJECT);
  ASSERT_TRUE(SerializeTraits<MemberCascadeSerializable>::SERIALIZABLE);
  MemberCascadeSerializable s {gen};
  ASSERT_TRUE(Serialization::print_to_string(s, string));
  BABYLON_LOG(INFO) << string;
}

struct OutterSerializable {
  struct InnerSerializable {
    int a {0};

    BABYLON_SERIALIZABLE((a, 1))
  };

  InnerSerializable s;

  BABYLON_SERIALIZABLE((s, 1))
};

TEST_F(AggregateTest, support_inner_class) {
  using S = OutterSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  s.s.a = gen();
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.s.a, ss.s.a);
}

class PrivateSerializable {
 public:
  int i {0};

 private:
  BABYLON_SERIALIZABLE((i, 1))
};

TEST_F(AggregateTest, support_use_in_private_section) {
  using S = PrivateSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s;
  s.i = gen();
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s.i, ss.i);
}

struct AutoTagSerializable {
  AutoTagSerializable() = default;
  AutoTagSerializable(std::mt19937_64& gen)
      : i(gen()), s {::std::to_string(gen())}, v(gen() % 10, gen()) {}

  bool operator==(const AutoTagSerializable& o) const {
    return i == o.i && s == o.s && v == o.v;
  }

  int i {0};
  ::std::string s;
  ::std::vector<int> v;

  BABYLON_SERIALIZABLE(i, s, v)
};
TEST_F(AggregateTest, support_auto_tag) {
  using S = AutoTagSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

struct AutoTagBaseCascadeSerializable : public BasicTypeSerializable {
  AutoTagBaseCascadeSerializable() = default;
  AutoTagBaseCascadeSerializable(::std::mt19937_64& gen)
      : BasicTypeSerializable {gen}, a(gen()) {}

  bool operator==(const AutoTagBaseCascadeSerializable& o) const {
    return static_cast<const BasicTypeSerializable&>(*this) == o && a == o.a;
  }

  int a {0};

  BABYLON_SERIALIZABLE_WITH_BASE(BasicTypeSerializable, a)
};
TEST_F(AggregateTest, support_auto_tag_with_base) {
  using S = AutoTagBaseCascadeSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

struct OldSerializable {
  OldSerializable() = default;
  OldSerializable(std::mt19937_64& gen)
      : i(gen()), s {::std::to_string(gen())}, v(gen() % 10, gen()) {}

  bool operator==(const OldSerializable& o) const {
    return i == o.i && s == o.s && v == o.v;
  }

  int i {0};
  ::std::string s;
  ::std::vector<int> v;

  BABYLON_SERIALIZABLE((i, 1)(s, 2)(v, 3))
};
TEST_F(AggregateTest, support_old_macro_name) {
  using S = OldSerializable;
  ASSERT_TRUE(SerializeTraits<S>::SERIALIZABLE);
  S s {gen};
  ASSERT_TRUE(Serialization::serialize_to_string(s, string));
  S ss;
  ASSERT_TRUE(Serialization::parse_from_string(string, ss));
  ASSERT_EQ(s, ss);
}

#endif // BABYLON_USE_PROTOBUF
