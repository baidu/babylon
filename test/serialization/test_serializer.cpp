#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "gtest/gtest.h"

#include <random>

using ::babylon::Any;
using ::babylon::Serialization;
using ::babylon::SerializationHelper;
using ::babylon::TypeId;

struct SerializerTest : public ::testing::Test {
    ::std::string string;
    ::std::mt19937_64 gen {::std::random_device{}()};
};

BABYLON_REGISTER_SERIALIZER(int);

TEST_F(SerializerTest, support_scalar) {
    ASSERT_EQ(nullptr, Serialization::serializer_for_name("float"));
    ASSERT_NE(nullptr, Serialization::serializer_for_name("int"));

    int s = gen();
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    Any ss;
    auto serializer = Serialization::serializer_for_name("int");
    ASSERT_TRUE(serializer->parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(TypeId<int>::ID, ss.instance_type());
    ASSERT_EQ(s, *ss.get<int>());
}

namespace user {
struct BasicTypeSerializable {
    BasicTypeSerializable() = default;
    BasicTypeSerializable(std::mt19937_64& gen) :
        i(gen()), s {::std::to_string(gen())}, v(gen() % 10, gen()) {}

    bool operator==(const BasicTypeSerializable& o) const {
        return i == o.i &&
               s == o.s &&
               v == o.v;
    }

    int i {0};
    ::std::string s;
    ::std::vector<int> v;

    BABYLON_SERIALIZABLE((i, 1)(s, 2)(v, 3))
}; 
BABYLON_REGISTER_SERIALIZER(BasicTypeSerializable);
}
TEST_F(SerializerTest, support_aggregate) {
    using S = ::user::BasicTypeSerializable;
    S s {gen};
    ASSERT_TRUE(Serialization::serialize_to_string(s, string));
    Any ss;
    auto serializer = Serialization::serializer_for_name("user::BasicTypeSerializable");
    ASSERT_NE(nullptr, serializer);
    ASSERT_TRUE(serializer->parse_from_string(string, ss));
    ASSERT_TRUE(ss);
    ASSERT_EQ(TypeId<S>::ID, ss.instance_type());
    ASSERT_EQ(s, *ss.get<S>());
}

#endif // BABYLON_USE_PROTOBUF
