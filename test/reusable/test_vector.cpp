#include "babylon/reusable/vector.h"

#include "babylon/reusable/string.h"

#include "gtest/gtest.h"

using ::babylon::MonotonicAllocator;
using ::babylon::ReusableTraits;
using ::babylon::Reuse;
#if BABYLON_USE_PROTOBUF
using ::babylon::Serialization;
using ::babylon::SerializeTraits;
#endif // BABYLON_USE_PROTOBUF
using ::babylon::SwissAllocator;
using ::babylon::SwissMemoryResource;
using ::babylon::SwissVector;
using ::babylon::SwissString;

struct ReusableVectorTest : public ::testing::Test {
    SwissMemoryResource resource;
    SwissAllocator<> allocator {resource};
    SwissMemoryResource other_resource;
    SwissAllocator<> other_allocator {other_resource};
    ::std::string long_string {::std::string(1024, 'x')};
};

TEST_F(ReusableVectorTest, trivial_as_element) {
    ASSERT_TRUE(::std::is_trivially_destructible<SwissVector<int>>::value);
    ASSERT_TRUE(::std::is_trivially_destructible<SwissVector<SwissString>>::value);
    struct S {
        ~S() {}
    };
    ASSERT_FALSE(::std::is_trivially_destructible<SwissVector<S>>::value);
}

TEST_F(ReusableVectorTest, constructible_with_allocator) {
    {
        SwissVector<int> v(allocator);
        ASSERT_TRUE(v.empty());
        ASSERT_EQ(0, v.capacity());
    }
    {
        SwissVector<int> v(12, allocator);
        ASSERT_EQ(12, v.size());
        ASSERT_EQ(12, v.constructed_size());
        ASSERT_EQ(12, v.capacity());
        for (auto i : v) {
            ASSERT_EQ(0, i);
        }
    }
    {
        SwissVector<int> v(12, 10086, allocator);
        ASSERT_EQ(12, v.size());
        ASSERT_EQ(12, v.constructed_size());
        ASSERT_EQ(12, v.capacity());
        for (auto i : v) {
            ASSERT_EQ(10086, i);
        }
    }
    {
        SwissVector<SwissString> v(12, "10086", allocator);
        ASSERT_EQ(12, v.size());
        ASSERT_EQ(12, v.constructed_size());
        ASSERT_EQ(12, v.capacity());
        for (auto& i : v) {
            ASSERT_EQ("10086", i);
        }
    }
    {
        int arr[] = {1, 2, 3};
        SwissVector<int> v(arr, arr + 3, allocator);
        ASSERT_EQ(3, v.size());
        ASSERT_EQ(3, v.constructed_size());
        ASSERT_EQ(3, v.capacity());
        ASSERT_EQ(1, v[0]);
        ASSERT_EQ(2, v[1]);
        ASSERT_EQ(3, v[2]);
    }
    {
        SwissVector<int> v({4, 5, 6}, allocator);
        ASSERT_EQ(3, v.size());
        ASSERT_EQ(3, v.constructed_size());
        ASSERT_EQ(3, v.capacity());
        ASSERT_EQ(4, v[0]);
        ASSERT_EQ(5, v[1]);
        ASSERT_EQ(6, v[2]);
    }
    {
        SwissVector<SwissString> v({"4", "5", "6"}, allocator);
        ASSERT_EQ(3, v.size());
        ASSERT_EQ(3, v.constructed_size());
        ASSERT_EQ(3, v.capacity());
        ASSERT_EQ("4", v[0]);
        ASSERT_EQ("5", v[1]);
        ASSERT_EQ("6", v[2]);
    }
}

TEST_F(ReusableVectorTest, elements_uses_allocator) {
    using V = SwissVector<SwissString>;
    ::std::vector<V, SwissAllocator<V>> vvs {allocator};
    vvs.emplace_back(::std::initializer_list<::std::string>{
            long_string + "10086"});
    auto& vs = vvs.back();
    vs.emplace_back(long_string + "10087");
    ASSERT_TRUE(resource.contains(&vs));
    ASSERT_TRUE(resource.contains(&vs[0]));
    ASSERT_EQ(long_string + "10086", vs[0]);
    ASSERT_TRUE(resource.contains(&vs[1]));
    ASSERT_EQ(long_string + "10087", vs[1]);

    vs.assign({long_string + "10010", long_string + "10011",
               long_string + "10012"});
    ASSERT_EQ(3, vs.size());
    ASSERT_TRUE(resource.contains(&vs[0]));
    ASSERT_EQ(long_string + "10010", vs[0]);
    ASSERT_TRUE(resource.contains(&vs[1]));
    ASSERT_EQ(long_string + "10011", vs[1]);
    ASSERT_TRUE(resource.contains(&vs[2]));
    ASSERT_EQ(long_string + "10012", vs[2]);
}

TEST_F(ReusableVectorTest, copyable) {
    SwissVector<SwissString> vs {allocator};
    vs.emplace_back("10086");
    vs.emplace_back("10010");

    SwissVector<SwissString> copyed_vs {vs};
    ASSERT_EQ(vs, copyed_vs);

    SwissVector<SwissString> copy_assigned_vs {allocator};
    ASSERT_NE(vs, copy_assigned_vs);
    copy_assigned_vs = copyed_vs;
    ASSERT_EQ(vs, copy_assigned_vs);
}

TEST_F(ReusableVectorTest, moveable) {
    SwissVector<SwissString> vs {allocator};
    vs.emplace_back(long_string + "10086");
    vs.emplace_back(long_string + "10010");
    SwissVector<SwissString> copyed_vs {vs};
    SwissString* ptrs[] = {&vs[0], &vs[1]};
    const char* sptrs[] = {vs[0].c_str(), vs[1].c_str()};

    SwissVector<SwissString> moved_vs {::std::move(vs)};
    ASSERT_TRUE(vs.empty());
    ASSERT_EQ(copyed_vs, moved_vs);
    ASSERT_EQ(ptrs[0], &moved_vs[0]);
    ASSERT_EQ(sptrs[0], moved_vs[0].c_str());
    ASSERT_EQ(sptrs[1], moved_vs[1].c_str());

    SwissVector<SwissString> move_assigned_vs {allocator};
    move_assigned_vs = ::std::move(moved_vs);
    ASSERT_TRUE(moved_vs.empty());
    ASSERT_EQ(copyed_vs, move_assigned_vs);
    ASSERT_EQ(ptrs[0], &move_assigned_vs[0]);
    ASSERT_EQ(ptrs[1], &move_assigned_vs[1]);
    ASSERT_EQ(sptrs[0], move_assigned_vs[0].c_str());
    ASSERT_EQ(sptrs[1], move_assigned_vs[1].c_str());
}

TEST_F(ReusableVectorTest, moveable_between_resource_downgrade_to_copy) {
    SwissVector<SwissString> vs {allocator};
    vs.emplace_back(long_string + "10086");
    vs.emplace_back(long_string + "10010");
    SwissVector<SwissString> copyed_vs {vs};
#if __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI
    vs[0][0];
    vs[1][0];
#endif // __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI
    SwissString* ptrs[] = {&vs[0], &vs[1]};
    const char* sptrs[] = {vs[0].c_str(), vs[1].c_str()};

    SwissVector<SwissString> moved_vs {::std::move(vs), other_allocator};
    ASSERT_EQ(copyed_vs, vs);
    ASSERT_EQ(copyed_vs, moved_vs);
    ASSERT_EQ(ptrs[0], &vs[0]);
    ASSERT_EQ(ptrs[1], &vs[1]);
    ASSERT_EQ(sptrs[0], vs[0].c_str());
    ASSERT_EQ(sptrs[1], vs[1].c_str());
    ASSERT_NE(ptrs[0], &moved_vs[0]);
    ASSERT_NE(ptrs[1], &moved_vs[1]);
    ASSERT_NE(sptrs[0], moved_vs[0].c_str());
    ASSERT_NE(sptrs[1], moved_vs[1].c_str());

    SwissVector<SwissString> move_assigned_vs {other_allocator};
    move_assigned_vs = ::std::move(vs);
    ASSERT_EQ(copyed_vs, vs);
    ASSERT_EQ(copyed_vs, moved_vs);
    ASSERT_EQ(ptrs[0], &vs[0]);
    ASSERT_EQ(ptrs[1], &vs[1]);
    ASSERT_EQ(sptrs[0], vs[0].c_str());
    ASSERT_EQ(sptrs[1], vs[1].c_str());
    ASSERT_NE(ptrs[0], &move_assigned_vs[0]);
    ASSERT_NE(ptrs[1], &move_assigned_vs[1]);
    ASSERT_NE(sptrs[0], move_assigned_vs[0].c_str());
    ASSERT_NE(sptrs[1], move_assigned_vs[1].c_str());
}

TEST_F(ReusableVectorTest, partially_moveable) {
    SwissVector<::std::string> vs {allocator};
    vs.emplace_back(long_string + "10086");
    vs.emplace_back(long_string + "10010");
    SwissVector<::std::string> copyed_vs {vs};
    ::std::string* ptrs[] = {&vs[0], &vs[1]};
    const char* sptrs[] = {vs[0].c_str(), vs[1].c_str()};

    SwissVector<::std::string> moved_vs {::std::move(vs), other_allocator};
    ASSERT_NE(copyed_vs, vs);
    ASSERT_EQ(copyed_vs.size(), vs.size());
    ASSERT_EQ(copyed_vs, moved_vs);
    ASSERT_EQ(ptrs[0], &vs[0]);
    ASSERT_EQ(ptrs[1], &vs[1]);
    ASSERT_TRUE(vs[0].empty());
    ASSERT_TRUE(vs[1].empty());
    ASSERT_NE(ptrs[0], &moved_vs[0]);
    ASSERT_NE(ptrs[1], &moved_vs[1]);
    ASSERT_EQ(sptrs[0], moved_vs[0].c_str());
    ASSERT_EQ(sptrs[1], moved_vs[1].c_str());

    ptrs[0] = &moved_vs[0];
    ptrs[1] = &moved_vs[1];
    SwissVector<::std::string> move_assigned_vs {allocator};
    move_assigned_vs = ::std::move(moved_vs);
    ASSERT_NE(copyed_vs, moved_vs);
    ASSERT_EQ(copyed_vs.size(), moved_vs.size());
    ASSERT_EQ(copyed_vs, move_assigned_vs);
    ASSERT_EQ(ptrs[0], &moved_vs[0]);
    ASSERT_EQ(ptrs[1], &moved_vs[1]);
    ASSERT_TRUE(moved_vs[0].empty());
    ASSERT_TRUE(moved_vs[1].empty());
    ASSERT_NE(ptrs[0], &move_assigned_vs[0]);
    ASSERT_NE(ptrs[1], &move_assigned_vs[1]);
    ASSERT_EQ(sptrs[0], move_assigned_vs[0].c_str());
    ASSERT_EQ(sptrs[1], move_assigned_vs[1].c_str());
}

TEST_F(ReusableVectorTest, random_accessable) {
    SwissVector<SwissString> vs {{"10010", "10086", "10016"}, allocator};
    ASSERT_EQ("10010", vs[0]);
    ASSERT_EQ("10086", vs[1]);
    ASSERT_EQ("10016", vs[2]);
    ASSERT_EQ("10010", vs.data()[0]);
    ASSERT_EQ("10086", vs.data()[1]);
    ASSERT_EQ("10016", vs.data()[2]);
    ASSERT_EQ("10010", vs.front());
    ASSERT_EQ("10016", vs.back());
    {
        const auto& cvs = vs;
        ASSERT_EQ("10010", cvs[0]);
        ASSERT_EQ("10086", cvs[1]);
        ASSERT_EQ("10016", cvs[2]);
        ASSERT_EQ("10010", cvs.data()[0]);
        ASSERT_EQ("10086", cvs.data()[1]);
        ASSERT_EQ("10016", cvs.data()[2]);
        ASSERT_EQ("10010", cvs.front());
        ASSERT_EQ("10016", cvs.back());
    }
}

TEST_F(ReusableVectorTest, iterable) {
    SwissVector<SwissString> vs {{"10010", "10086", "10016"}, allocator};
    ASSERT_EQ("10010", *vs.begin());
    ASSERT_EQ("10086", *(vs.begin() + 1));
    ASSERT_EQ("10016", *(vs.begin() + 2));
    ASSERT_EQ(vs.end(), vs.begin() + 3);
    ASSERT_EQ("10010", *vs.cbegin());
    ASSERT_EQ("10086", *(vs.cbegin() + 1));
    ASSERT_EQ("10016", *(vs.cbegin() + 2));
    ASSERT_EQ(vs.cend(), vs.cbegin() + 3);
    ASSERT_EQ("10016", *vs.rbegin());
    ASSERT_EQ("10086", *(vs.rbegin() + 1));
    ASSERT_EQ("10010", *(vs.rbegin() + 2));
    ASSERT_EQ(vs.rend(), vs.rbegin() + 3);
    ASSERT_EQ("10016", *vs.crbegin());
    ASSERT_EQ("10086", *(vs.crbegin() + 1));
    ASSERT_EQ("10010", *(vs.crbegin() + 2));
    ASSERT_EQ(vs.crend(), vs.crbegin() + 3);
    {
        const auto& cvs = vs;
        ASSERT_EQ("10010", *cvs.begin());
        ASSERT_EQ("10086", *(cvs.begin() + 1));
        ASSERT_EQ("10016", *(cvs.begin() + 2));
        ASSERT_EQ(vs.end(), cvs.begin() + 3);
        ASSERT_EQ("10010", *cvs.cbegin());
        ASSERT_EQ("10086", *(cvs.cbegin() + 1));
        ASSERT_EQ("10016", *(cvs.cbegin() + 2));
        ASSERT_EQ(cvs.cend(), cvs.cbegin() + 3);
        ASSERT_EQ("10016", *cvs.rbegin());
        ASSERT_EQ("10086", *(cvs.rbegin() + 1));
        ASSERT_EQ("10010", *(cvs.rbegin() + 2));
        ASSERT_EQ(cvs.rend(), cvs.rbegin() + 3);
        ASSERT_EQ("10016", *cvs.crbegin());
        ASSERT_EQ("10086", *(cvs.crbegin() + 1));
        ASSERT_EQ("10010", *(cvs.crbegin() + 2));
        ASSERT_EQ(cvs.crend(), cvs.crbegin() + 3);
    }
}

TEST_F(ReusableVectorTest, reserve_grow_only) {
    SwissVector<SwissString> vs {allocator};
    ASSERT_EQ(0, vs.capacity());
    vs.reserve(100);
    ASSERT_EQ(100, vs.capacity());
    vs.reserve(30);
    ASSERT_EQ(100, vs.capacity());
    vs.reserve(130);
    ASSERT_EQ(130, vs.capacity());
}

TEST_F(ReusableVectorTest, clear_keep_instance) {
    SwissVector<SwissString> vs {allocator};
    ASSERT_EQ(0, vs.size());
    ASSERT_EQ(0, vs.constructed_size());
    ASSERT_EQ(0, vs.capacity());
    vs.reserve(4);
    vs.emplace_back(::std::string(1000, 'x'));
    vs.emplace_back(::std::string(2000, 'y'));
    ASSERT_EQ(2, vs.size());
    ASSERT_EQ(2, vs.constructed_size());
    ASSERT_EQ(4, vs.capacity());
    size_t caps[] = {vs[0].capacity(), vs[1].capacity()};
    vs.clear();
    ASSERT_EQ(0, vs.size());
    ASSERT_EQ(2, vs.constructed_size());
    ASSERT_EQ(4, vs.capacity());
    vs.emplace_back("10010");
    vs.emplace_back("10086");
    vs.emplace_back("10016");
    ASSERT_EQ(3, vs.size());
    ASSERT_EQ(3, vs.constructed_size());
    ASSERT_EQ(4, vs.capacity());
    ASSERT_EQ(caps[0], vs[0].capacity());
    ASSERT_EQ(caps[1], vs[1].capacity());
    ASSERT_LE(5, vs[2].capacity());
    ASSERT_GT(100, vs[2].capacity());
}

TEST_F(ReusableVectorTest, insert_element) {
    SwissVector<SwissString> vs {allocator};
    vs.insert(vs.begin(), "10086");
    vs.insert(vs.begin() + 1, "10010");
    vs.insert(vs.begin() + 1, "10016");
    ASSERT_EQ("10086", vs[0]);
    ASSERT_EQ("10016", vs[1]);
    ASSERT_EQ("10010", vs[2]);

    vs.insert(vs.begin() + 3, "8610086");
    vs.insert(vs.begin(), "8610010");
    vs.insert(vs.begin() + 1, "8610016");
    ASSERT_EQ("8610010", vs[0]);
    ASSERT_EQ("8610016", vs[1]);
    ASSERT_EQ("10086", vs[2]);
    ASSERT_EQ("10016", vs[3]);
    ASSERT_EQ("10010", vs[4]);
    ASSERT_EQ("8610086", vs[5]);
}

TEST_F(ReusableVectorTest, insert_move_element) {
    SwissVector<SwissString> vs {allocator};
    SwissString s {long_string + "10086", allocator};
#if __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI
    s[0];
#endif // __GLIBCXX__ && !_GLIBCXX_USE_CXX11_ABI
    auto ptr = s.c_str();
    vs.insert(vs.begin(), s);
    ASSERT_FALSE(s.empty());
    ASSERT_EQ(long_string + "10086", vs[0]);
    ASSERT_NE(ptr, vs[0].c_str());
    vs.insert(vs.begin(), ::std::move(s));
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(long_string + "10086", vs[0]);
    ASSERT_EQ(ptr, vs[0].c_str());
}

TEST_F(ReusableVectorTest, insert_range) {
    SwissVector<SwissString> vs {allocator};
    vs.insert(vs.begin(), 3, "10086");
    ASSERT_EQ(3, vs.size());
    ASSERT_EQ("10086", vs[0]);
    ASSERT_EQ("10086", vs[1]);
    ASSERT_EQ("10086", vs[2]);

    ::std::vector<::std::string> svs {"10010", "10016"};
    vs.insert(vs.begin() + 1, svs.begin(), svs.end());
    ASSERT_EQ(5, vs.size());
    ASSERT_EQ("10086", vs[0]);
    ASSERT_EQ("10010", vs[1]);
    ASSERT_EQ("10016", vs[2]);
    ASSERT_EQ("10086", vs[3]);
    ASSERT_EQ("10086", vs[4]);

    vs.insert(vs.begin() + 1, {"8610010", "8610016"});
    ASSERT_EQ(7, vs.size());
    ASSERT_EQ("10086", vs[0]);
    ASSERT_EQ("8610010", vs[1]);
    ASSERT_EQ("8610016", vs[2]);
    ASSERT_EQ("10010", vs[3]);
    ASSERT_EQ("10016", vs[4]);
    ASSERT_EQ("10086", vs[5]);
    ASSERT_EQ("10086", vs[6]);
}

TEST_F(ReusableVectorTest, erase_keep_instance) {
    SwissVector<SwissString> vs {{"10086", "10010", "10016"}, allocator};
    vs[0].reserve(1024);
    vs[1].reserve(1024);
    vs[2].reserve(1024);

    vs.erase(vs.begin() + 1);
    ASSERT_EQ(2, vs.size());
    ASSERT_EQ(3, vs.constructed_size());
    ASSERT_EQ("10086", vs[0]);
    ASSERT_EQ("10016", vs[1]);

    vs.erase(vs.begin() + 1);
    ASSERT_EQ(1, vs.size());
    ASSERT_EQ(3, vs.constructed_size());
    ASSERT_EQ("10086", vs[0]);

    vs.erase(vs.begin());
    ASSERT_EQ(0, vs.size());
    ASSERT_EQ(3, vs.constructed_size());

    vs.insert(vs.begin(), {
        "10016", "8610010", "10086",
        "8610016", "10010", "8610086"});
    ASSERT_LE(1024, vs[0].capacity());
    ASSERT_LE(1024, vs[1].capacity());
    ASSERT_LE(1024, vs[2].capacity());
    ASSERT_GT(1024, vs[3].capacity());
    ASSERT_GT(1024, vs[4].capacity());
    ASSERT_GT(1024, vs[5].capacity());

    vs.erase(vs.begin() + 2, vs.begin() + 4);
    ASSERT_EQ(4, vs.size());
    ASSERT_EQ("10016", vs[0]);
    ASSERT_EQ("8610010", vs[1]);
    ASSERT_EQ("10010", vs[2]);
    ASSERT_EQ("8610086", vs[3]);

    vs.erase(vs.begin() + 2, vs.begin() + 4);
    ASSERT_EQ(2, vs.size());
    ASSERT_EQ("10016", vs[0]);
    ASSERT_EQ("8610010", vs[1]);

    vs.erase(vs.begin(), vs.begin() + 2);
    ASSERT_EQ(0, vs.size());
}

TEST_F(ReusableVectorTest, resize_keep_instance) {
    SwissVector<SwissString> vs {{"10086", "10010", "10016"}, allocator};
    vs[0].reserve(1024);
    vs[1].reserve(1024);
    vs[2].reserve(1024);

    vs.resize(1);
    vs.resize(4, "8610010");
    ASSERT_EQ(4, vs.size());
    ASSERT_EQ("10086", vs[0]);
    ASSERT_EQ("8610010", vs[1]);
    ASSERT_EQ("8610010", vs[2]);
    ASSERT_EQ("8610010", vs[3]);
    ASSERT_LE(1024, vs[0].capacity());
    ASSERT_LE(1024, vs[1].capacity());
    ASSERT_LE(1024, vs[2].capacity());
    ASSERT_GT(1024, vs[3].capacity());
}

TEST_F(ReusableVectorTest, swapable) {
    SwissVector<SwissString> vs {{"10086", "10010", "10016"}, allocator};
    SwissVector<SwissString> ovs {allocator};
    auto pvs = vs.data();
    auto povs = ovs.data();
    swap(vs, ovs);
    ASSERT_EQ(pvs, ovs.data());
    ASSERT_EQ(povs, vs.data());

    ASSERT_TRUE(vs.empty());
    ASSERT_EQ(3, ovs.size());
    ASSERT_EQ("10086", ovs[0]);
    ASSERT_EQ("10010", ovs[1]);
    ASSERT_EQ("10016", ovs[2]);
}

TEST_F(ReusableVectorTest, reusable) {
    SwissVector<SwissString> vs {{"10086", "10010", "10016"}, allocator};
    vs[0].reserve(1024);

    Reuse::AllocationMetadata<SwissVector<SwissString>> meta;
    Reuse::update_allocation_metadata(vs, meta);
    auto& rvs =
        *Reuse::create_with_allocation_metadata<SwissVector<SwissString>>(
            allocator, meta);
    ASSERT_TRUE(rvs.empty());
    ASSERT_EQ(3, rvs.constructed_size());

    Reuse::reconstruct(rvs, allocator);
    ASSERT_TRUE(rvs.empty());
    ASSERT_EQ(3, rvs.constructed_size());

    Reuse::reconstruct(rvs, allocator, 2);
    ASSERT_EQ(2, rvs.size());
    ASSERT_TRUE(rvs[0].empty());
    ASSERT_TRUE(rvs[1].empty());
    ASSERT_LE(1024, rvs[0].capacity());
    ASSERT_LE(1024, rvs[1].capacity());
    ASSERT_EQ(3, rvs.constructed_size());

    Reuse::reconstruct(rvs, allocator, 2, "10086");
    ASSERT_EQ(2, rvs.size());
    ASSERT_EQ("10086", rvs[0]);
    ASSERT_EQ("10086", rvs[1]);
    ASSERT_LE(1024, rvs[0].capacity());
    ASSERT_LE(1024, rvs[1].capacity());
    ASSERT_EQ(3, rvs.constructed_size());

    ::std::vector<::std::string> svs {"10010", "10016"};
    Reuse::reconstruct(rvs, allocator, svs.begin(), svs.end());
    ASSERT_EQ(2, rvs.size());
    ASSERT_EQ("10010", rvs[0]);
    ASSERT_EQ("10016", rvs[1]);
    ASSERT_LE(1024, rvs[0].capacity());
    ASSERT_LE(1024, rvs[1].capacity());
    ASSERT_EQ(3, rvs.constructed_size());

    Reuse::reconstruct(rvs, allocator,
        ::std::initializer_list<const char*>{"10016", "10010"});
    ASSERT_EQ(2, rvs.size());
    ASSERT_EQ("10016", rvs[0]);
    ASSERT_EQ("10010", rvs[1]);
    ASSERT_LE(1024, rvs[0].capacity());
    ASSERT_LE(1024, rvs[1].capacity());
    ASSERT_EQ(3, rvs.constructed_size());
}

#if BABYLON_USE_PROTOBUF
struct BasicReusableSerializable {
    struct AllocationMetadata {
    };

    BasicReusableSerializable() = default;
    BasicReusableSerializable(int x) : i {x} {}

    int i {0};

    BABYLON_COMPATIBLE((i, 1))
};
struct VectorReusableSerializable {
    using allocator_type = SwissAllocator<>;
    VectorReusableSerializable(allocator_type allocator) : vs {allocator}, ss {allocator} {}

    SwissVector<SwissString> vs;
    SwissVector<BasicReusableSerializable> ss;

    BABYLON_COMPATIBLE((vs, 1)(ss, 2))
};

TEST_F(ReusableVectorTest, serializable) {
    ASSERT_TRUE(SerializeTraits<SwissVector<SwissString>>::SERIALIZABLE);
    {
        struct S {
        };
        ASSERT_FALSE(SerializeTraits<SwissVector<S>>::SERIALIZABLE);
    }
    using S = VectorReusableSerializable;
    auto s = allocator.create_object<S>();
    s->vs.insert(s->vs.begin(), {"10010", "10086", "10016"});
    s->ss.insert(s->ss.begin(), {10010, 10086, 10016});

    ::std::string str;
    ASSERT_TRUE(Serialization::serialize_to_string(*s, str));

    auto ss = allocator.create_object<S>();
    ASSERT_TRUE(Serialization::parse_from_string(str, *ss));
    ASSERT_EQ(s->vs, ss->vs);

    ASSERT_TRUE(Serialization::print_to_string(*ss, str));
    ::std::cerr << str;
}
#endif // BABYLON_USE_PROTOBUF
