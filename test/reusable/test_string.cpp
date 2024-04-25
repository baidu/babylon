#include "babylon/reusable/string.h"

#include "babylon/serialization.h"

#include "gtest/gtest.h"

using ::babylon::MonotonicAllocator;
using ::babylon::MonotonicString;
using ::babylon::ReusableTraits;
using ::babylon::Reuse;
using ::babylon::StringView;
using ::babylon::SwissAllocator;
using ::babylon::SwissMemoryResource;
using ::babylon::SwissString;
#if BABYLON_USE_PROTOBUF
using ::babylon::Serialization;
using ::babylon::SerializeTraits;
#endif // BABYLON_USE_PROTOBUF

struct ReusableStringTest : public ::testing::Test {
    SwissMemoryResource resource;
    SwissAllocator<> allocator {resource};
    ::std::string long_string {::std::string(1024, 'x')};
};

TEST_F(ReusableStringTest, string_uses_allocator) {
    {
        auto s = allocator.create_object<SwissString>();
        ASSERT_TRUE(s->empty());
        ASSERT_TRUE(resource.contains(s));
    }
    {
        auto s = allocator.create_object<SwissString>(1024, 'y');
        ASSERT_EQ((::std::string(1024, 'y')), *s);
        ASSERT_TRUE(resource.contains(s));
        ASSERT_TRUE(resource.contains(s->c_str()));
    }
    {
        auto s = allocator.create_object<SwissString>(long_string.c_str());
        ASSERT_EQ(long_string, *s);
        ASSERT_TRUE(resource.contains(s));
        ASSERT_TRUE(resource.contains(s->c_str()));
    }
    {
        auto s = allocator.create_object<SwissString>(long_string.c_str(), 512);
        ASSERT_EQ(long_string.substr(0, 512), *s);
        ASSERT_TRUE(resource.contains(s));
        ASSERT_TRUE(resource.contains(s->c_str()));
    }
    {
        auto s = allocator.create_object<SwissString>(long_string);
        ASSERT_EQ(long_string, *s);
        ASSERT_TRUE(resource.contains(s));
        ASSERT_TRUE(resource.contains(s->c_str()));
    }
}

TEST_F(ReusableStringTest, reusable) {
    Reuse::AllocationMetadata<SwissString> meta;
    auto& s = *allocator.create_object<SwissString>();
    s.resize(10086);
    Reuse::reconstruct(s, allocator, "10086");
    ASSERT_EQ("10086", s);
    ASSERT_LE(10086, s.capacity());
    Reuse::reconstruct(s, allocator);
    ASSERT_LE(10086, s.capacity());
    ASSERT_TRUE(s.empty());
    Reuse::update_allocation_metadata(s, meta);
    {
        auto& ss = *allocator.allocate_object<SwissString>();
        Reuse::construct_with_allocation_metadata(&ss, allocator, meta);
        ASSERT_LE(10086, ss.capacity());
        ASSERT_TRUE(ss.empty());
        allocator.destroy(&ss);
    }
    {
        auto& ss = *Reuse::create_with_allocation_metadata<SwissString>(allocator, meta);
        ASSERT_LE(10086, ss.capacity());
        ASSERT_TRUE(ss.empty());
    }
}

#if BABYLON_USE_PROTOBUF
struct StringMemberSerializable {
    using allocator_type = SwissAllocator<>;
    StringMemberSerializable(allocator_type allocator) : s {allocator} {}

    SwissString s;

    BABYLON_SERIALIZABLE((s, 1))
};
TEST_F(ReusableStringTest, serializable) {
    using S = StringMemberSerializable;
    ASSERT_TRUE(SerializeTraits<SwissString>::SERIALIZABLE);

    auto s = allocator.create_object<S>();
    s->s = long_string;

    ::std::string str;
    ASSERT_TRUE(Serialization::serialize_to_string(*s, str));

    auto ss = allocator.create_object<S>();
    ASSERT_TRUE(Serialization::parse_from_string(str, *ss));
    ASSERT_EQ(long_string, ss->s);

    ASSERT_TRUE(Serialization::print_to_string(*ss, str));
    ::std::cerr << str;
}
#endif // BABYLON_USE_PROTOBUF

#if __cplusplus >= 201703L
TEST_F(ReusableStringTest, convertible_from_string_with_default_allocator) {
    SwissMemoryResource resource;
    ::std::string s(10086, 'x');

    {
        MonotonicString ss {s, resource};
        ASSERT_EQ(::std::string_view {s}, ss);
        ss.clear();
        ss = s;
        ASSERT_EQ(::std::string_view {s}, ss);
    }
    {
        SwissString ss {s, resource};
        ASSERT_EQ(::std::string_view {s}, ss);
        ss.clear();
        ss = s;
        ASSERT_EQ(::std::string_view {s}, ss);
    }
}
#endif // __cplusplus >= 201703L

TEST_F(ReusableStringTest, convertible_from_string_view) {
    SwissMemoryResource resource;
    ::std::string s(10086, 'x');
    ::std::string_view sv {s};
    StringView bsv {s};

    {
        MonotonicString ss {sv, resource};
        ASSERT_EQ(sv, ss);
        ss.clear();
        ss = sv;
        ASSERT_EQ(sv, ss);
    }
    {
        SwissString ss {bsv, resource};
        ASSERT_EQ(bsv, ss);
        ss.clear();
        ss = bsv;
        ASSERT_EQ(bsv, ss);
    }
}

TEST_F(ReusableStringTest, trivially_destructible) {
    ASSERT_FALSE(::std::is_trivially_destructible<::std::string>::value);
    ASSERT_TRUE(::std::is_trivially_destructible<MonotonicString>::value);
    ASSERT_TRUE(::std::is_trivially_destructible<SwissString>::value);

    SwissMemoryResource resource;
    ::std::string s(10086, 'x');
    ::std::string_view sv {s};

    {
        auto string = MonotonicAllocator<> {resource}.new_object<MonotonicString>(sv);
        ASSERT_EQ(sv, *string);
        MonotonicAllocator<> {resource}.delete_object(string);
    }
    {
        auto string = MonotonicAllocator<> {resource}.new_object<MonotonicString>(sv);
        ASSERT_EQ(sv, *string);
    }
    {
        auto string = SwissAllocator<> {resource}.new_object<SwissString>(sv);
        ASSERT_EQ(sv, *string);
        SwissAllocator<> {resource}.delete_object(string);
    }
    {
        auto string = SwissAllocator<> {resource}.new_object<SwissString>(sv);
        ASSERT_EQ(sv, *string);
    }
}

TEST_F(ReusableStringTest, support_resize_uninitialized) {
    auto& str = *allocator.create_object<SwissString>("10086");
    auto* data = resize_uninitialized(str, 4);
    ASSERT_EQ(4, str.size());
    ASSERT_EQ("1008", str);
    ASSERT_EQ(data, str.data());
    data = resize_uninitialized(str, 2);
    ASSERT_EQ(2, str.size());
    ASSERT_EQ("10", str);
    ASSERT_EQ(data, str.data());
    data = resize_uninitialized(str, 4);
    ASSERT_EQ(4, str.size());
    ASSERT_EQ(::std::string_view("10\08", 4), str);
    ASSERT_EQ(data, str.data());
}
