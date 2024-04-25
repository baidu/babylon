#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "gtest/gtest.h"

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializationHelper;
using ::babylon::SerializeTraits;
using ::google::protobuf::internal::WireFormatLite;

struct ScalarTest : public ::testing::Test {
    ::std::string string;
    ::std::mt19937_64 gen {::std::random_device{}()};
};

enum TestEnum {
    A, B, C,
};

enum class TestEnumClass {
    A, B, C,
};

TEST_F(ScalarTest, serializable) {
#define TEST_FOR(type, complexity, wire_type) \
    { \
        ASSERT_TRUE(SerializeTraits<type>::SERIALIZABLE); \
        ASSERT_EQ(SerializeTraits<type>::WIRE_TYPE, \
                  WireFormatLite::WIRETYPE_##wire_type); \
        ASSERT_FALSE(SerializeTraits<type>::SERIALIZED_SIZE_CACHED); \
        ASSERT_EQ(SerializeTraits<type>::SERIALIZED_SIZE_COMPLEXITY, \
                  SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_##complexity); \
        ASSERT_FALSE(SerializeTraits<type>::PRINT_AS_OBJECT); \
        type s = static_cast<type>(gen()); \
        ASSERT_TRUE(Serialization::serialize_to_string(s, string)); \
        type ss; \
        ASSERT_TRUE(Serialization::parse_from_string(string, ss)); \
        ASSERT_EQ(s, ss); \
    }
    TEST_FOR(bool, SIMPLE, VARINT)
    TEST_FOR(int8_t, SIMPLE, VARINT)
    TEST_FOR(int16_t, SIMPLE, VARINT)
    TEST_FOR(int32_t, SIMPLE, VARINT)
    TEST_FOR(int64_t, SIMPLE, VARINT)
    TEST_FOR(uint8_t, SIMPLE, VARINT)
    TEST_FOR(uint16_t, SIMPLE, VARINT)
    TEST_FOR(uint32_t, SIMPLE, VARINT)
    TEST_FOR(uint64_t, SIMPLE, VARINT)
    TEST_FOR(TestEnum, SIMPLE, VARINT)
    TEST_FOR(TestEnumClass, SIMPLE, VARINT)
    TEST_FOR(float, TRIVIAL, FIXED32)
    TEST_FOR(double, TRIVIAL, FIXED64)
#undef TEST_FOR
}

#endif // BABYLON_USE_PROTOBUF
