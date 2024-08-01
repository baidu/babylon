#include "babylon/reusable/allocator.h"
#include "babylon/reusable/traits.h"

#include <gtest/gtest.h>

#if BABYLON_USE_PROTOBUF
#include <arena_example.pb.h>
#endif // BABYLON_USE_PROTOBUF

using ::babylon::ReusableTraits;
using ::babylon::Reuse;
using ::babylon::SwissAllocator;
using ::babylon::SwissMemoryResource;
#if BABYLON_USE_PROTOBUF
using ::babylon::TestMessage;
#endif // BABYLON_USE_PROTOBUF

struct ReusableTraitsTest : public ::testing::Test {
  SwissMemoryResource resource;
  SwissAllocator<> allocator {resource};
};

/*
TEST_F(ReusableTraitsTest, scalar_is_reusable) {
  ASSERT_TRUE(ReusableTraits<int>::REUSABLE);
  ASSERT_EQ(0, sizeof(ReusableTraits<int>::AllocationMetadata));
  Reuse::AllocationMetadata<int> meta;
  int x = 10086;
  Reuse::reconstruct(x, allocator);
  ASSERT_EQ(0, x);
  x = 10086;
  Reuse::update_allocation_metadata(x, meta);
  ASSERT_EQ(10086, x);
  Reuse::construct_with_allocation_metadata(&x, allocator, meta);
  ASSERT_EQ(0, x);
  auto y = Reuse::create_with_allocation_metadata<int>(allocator, meta);
  ASSERT_NE(nullptr, y);
  ASSERT_EQ(0, *y);
}

TEST_F(ReusableTraitsTest, pod_is_reusable) {
  struct S {
    int a;
    int b;
  };
  ASSERT_TRUE(ReusableTraits<S>::REUSABLE);
  ASSERT_EQ(0, sizeof(ReusableTraits<S>::AllocationMetadata));
  Reuse::AllocationMetadata<S> meta;
  S s {.a = 10086, .b = 10010};
  Reuse::reconstruct(s, allocator);
  ASSERT_EQ(0, s.a);
  ASSERT_EQ(0, s.b);
  s.a = 10086;
  s.b = 10010;
  Reuse::update_allocation_metadata(s, meta);
  ASSERT_EQ(10086, s.a);
  ASSERT_EQ(10010, s.b);
  Reuse::construct_with_allocation_metadata(&s, allocator, meta);
  ASSERT_EQ(0, s.a);
  ASSERT_EQ(0, s.b);
  auto ss = Reuse::create_with_allocation_metadata<S>(allocator, meta);
  ASSERT_NE(nullptr, ss);
  ASSERT_EQ(0, ss->a);
  ASSERT_EQ(0, ss->b);
}

TEST_F(ReusableTraitsTest, normal_class_is_not_reusable) {
  struct S {
    int a {10010};
    int b {10086};
  };
  ASSERT_FALSE(ReusableTraits<S>::REUSABLE);
  ASSERT_EQ(0, sizeof(ReusableTraits<S>::AllocationMetadata));
  Reuse::AllocationMetadata<S> meta;
  S s;
  s.a = 10086;
  s.b = 10010;
  Reuse::reconstruct(s, allocator);
  ASSERT_EQ(10010, s.a);
  ASSERT_EQ(10086, s.b);
  s.a = 10086;
  s.b = 10010;
  Reuse::update_allocation_metadata(s, meta);
  ASSERT_EQ(10086, s.a);
  ASSERT_EQ(10010, s.b);
  Reuse::construct_with_allocation_metadata(&s, allocator, meta);
  ASSERT_EQ(10010, s.a);
  ASSERT_EQ(10086, s.b);
  auto ss = Reuse::create_with_allocation_metadata<S>(allocator, meta);
  ASSERT_NE(nullptr, ss);
}

TEST_F(ReusableTraitsTest, class_without_dynamic_content_can_be_reusable) {
  struct S {
    using AllocationMetadata = void;
    int a {10010};
    int b {10086};
  };
  ASSERT_TRUE(ReusableTraits<S>::REUSABLE);
  ASSERT_EQ(0, sizeof(ReusableTraits<S>::AllocationMetadata));
  Reuse::AllocationMetadata<S> meta;
  S s;
  s.a = 10086;
  s.b = 10010;
  Reuse::reconstruct(s, allocator);
  ASSERT_EQ(10010, s.a);
  ASSERT_EQ(10086, s.b);
  s.a = 10086;
  s.b = 10010;
  Reuse::update_allocation_metadata(s, meta);
  ASSERT_EQ(10086, s.a);
  ASSERT_EQ(10010, s.b);
  Reuse::construct_with_allocation_metadata(&s, allocator, meta);
  ASSERT_EQ(10010, s.a);
  ASSERT_EQ(10086, s.b);
  auto ss = Reuse::create_with_allocation_metadata<S>(allocator, meta);
  ASSERT_NE(nullptr, ss);
}

TEST_F(ReusableTraitsTest, class_with_dynamic_content_can_be_reusable) {
  struct S {
    using allocator_type = SwissAllocator<char>;
    struct AllocationMetadata {
      size_t capacity {0};
    };
    S(allocator_type allocator) : s {allocator} {}
    // S(const char* str, allocator_type allocator) : s {str, str + strlen(str),
    // allocator} {}
    S(const AllocationMetadata& meta, allocator_type allocator)
        : s {allocator} {
      s.reserve(meta.capacity);
    }
    void update_allocation_metadata(AllocationMetadata& meta) const {
      meta.capacity = s.capacity();
    }
    void assign(const char* str) {
      s.assign(str, str + strlen(str));
    }
    void clear() {
      s.clear();
    }

    ::std::vector<char, allocator_type> s;
  };
  ASSERT_TRUE(ReusableTraits<S>::REUSABLE);
  ASSERT_LT(0, sizeof(ReusableTraits<S>::AllocationMetadata));
  Reuse::AllocationMetadata<S> meta;
  auto& s = *allocator.create_object<S>();
  s.s.resize(10086);
  Reuse::reconstruct(s, allocator, "10086");
  ASSERT_EQ(0, strncmp("10086", s.s.data(), 5));
  ASSERT_EQ(10086, s.s.capacity());
  Reuse::reconstruct(s, allocator);
  ASSERT_EQ(10086, s.s.capacity());
  ASSERT_TRUE(s.s.empty());
  Reuse::update_allocation_metadata(s, meta);
  {
    auto& ss = *allocator.allocate_object<S>();
    Reuse::construct_with_allocation_metadata(&ss, allocator, meta);
    ASSERT_EQ(10086, ss.s.capacity());
    ASSERT_TRUE(ss.s.empty());
    allocator.destroy(&ss);
  }
  {
    auto& ss = *Reuse::create_with_allocation_metadata<S>(allocator, meta);
    ASSERT_EQ(10086, ss.s.capacity());
    ASSERT_TRUE(ss.s.empty());
  }
}

TEST_F(ReusableTraitsTest, string_is_reusable) {
  using S = ::std::string;
  ASSERT_TRUE(ReusableTraits<S>::REUSABLE);
  ASSERT_LT(0, sizeof(ReusableTraits<S>::AllocationMetadata));
  Reuse::AllocationMetadata<S> meta;
  auto& s = *allocator.create_object<S>();
  s.resize(10086);
  Reuse::reconstruct(s, allocator, "10086");
  ASSERT_EQ("10086", s);
  ASSERT_LE(10086, s.capacity());
  Reuse::reconstruct(s, allocator);
  ASSERT_LE(10086, s.capacity());
  ASSERT_TRUE(s.empty());
  Reuse::update_allocation_metadata(s, meta);
  {
    auto& ss = *allocator.allocate_object<S>();
    Reuse::construct_with_allocation_metadata(&ss, allocator, meta);
    ASSERT_LE(10086, ss.capacity());
    ASSERT_TRUE(ss.empty());
    allocator.destroy(&ss);
  }
  {
    auto& ss = *Reuse::create_with_allocation_metadata<S>(allocator, meta);
    ASSERT_LE(10086, ss.capacity());
    ASSERT_TRUE(ss.empty());
  }
}

#if __cpp_lib_memory_resource >= 201603L && \
    (!__GLIBCXX__ || _GLIBCXX_USE_CXX11_ABI)
TEST_F(ReusableTraitsTest, pmr_string_is_reusable) {
  using S = ::std::pmr::string;
  ASSERT_TRUE(ReusableTraits<S>::REUSABLE);
  ASSERT_LT(0, sizeof(ReusableTraits<S>::AllocationMetadata));
  Reuse::AllocationMetadata<S> meta;
  auto& s = *allocator.create_object<S>();
  s.resize(10086);
  Reuse::reconstruct(s, allocator, "10086");
  ASSERT_EQ("10086", s);
  ASSERT_EQ(10086, s.capacity());
  Reuse::reconstruct(s, allocator);
  ASSERT_EQ(10086, s.capacity());
  ASSERT_TRUE(s.empty());
  Reuse::update_allocation_metadata(s, meta);
  {
    auto& ss = *allocator.allocate_object<S>();
    Reuse::construct_with_allocation_metadata(&ss, allocator, meta);
    ASSERT_EQ(10086, ss.capacity());
    ASSERT_TRUE(ss.empty());
    allocator.destroy(&ss);
  }
  {
    auto& ss = *Reuse::create_with_allocation_metadata<S>(allocator, meta);
    ASSERT_EQ(10086, ss.capacity());
    ASSERT_TRUE(ss.empty());
  }
}
#endif // __cpp_lib_memory_resource >= 201603L && (!__GLIBCXX__ ||
       // _GLIBCXX_USE_CXX11_ABI)

TEST_F(ReusableTraitsTest, scala_vector_is_reusable) {
  ASSERT_TRUE(ReusableTraits<::std::vector<int>>::REUSABLE);
  ASSERT_LT(0, sizeof(ReusableTraits<::std::vector<int>>::AllocationMetadata));
  Reuse::AllocationMetadata<::std::vector<int>> meta;
  auto& s = *allocator.create_object<::std::vector<int>>();
  s.resize(10086);
  Reuse::reconstruct(s, allocator, ::std::initializer_list<int> {10010, 10086});
  ASSERT_EQ((::std::vector<int> {10010, 10086}), s);
  ASSERT_EQ(10086, s.capacity());
  Reuse::reconstruct(s, allocator);
  ASSERT_EQ(10086, s.capacity());
  ASSERT_TRUE(s.empty());
  Reuse::update_allocation_metadata(s, meta);
  {
    auto& ss = *allocator.allocate_object<::std::vector<int>>();
    Reuse::construct_with_allocation_metadata(&ss, allocator, meta);
    ASSERT_EQ(10086, ss.capacity());
    ASSERT_TRUE(ss.empty());
    allocator.destroy(&ss);
  }
  {
    auto& ss = *Reuse::create_with_allocation_metadata<::std::vector<int>>(
        allocator, meta);
    ASSERT_EQ(10086, ss.capacity());
    ASSERT_TRUE(ss.empty());
  }
}
*/

#if BABYLON_USE_PROTOBUF
TEST_F(ReusableTraitsTest, message_reusable) {
  ASSERT_TRUE(ReusableTraits<TestMessage>::REUSABLE);
  ASSERT_LT(0, sizeof(ReusableTraits<TestMessage>::AllocationMetadata));
  Reuse::AllocationMetadata<TestMessage> meta;
  auto& m = *allocator.create_object<TestMessage>();
  ::std::cerr << "arena " << m.GetArena() << ::std::endl;
  m.mutable_s()->reserve(10086);
  ::std::cerr << "after arena " << m.GetArena() << ::std::endl;
  Reuse::reconstruct(m, allocator);
  ASSERT_TRUE(m.s().empty());
  ASSERT_LE(10086, m.s().capacity());
  Reuse::update_allocation_metadata(m, meta);
  {
    auto& mm =
        *Reuse::create_with_allocation_metadata<TestMessage>(allocator, meta);
    ASSERT_LE(10086, mm.s().capacity());
    ASSERT_TRUE(mm.s().empty());
  }
}

/*
TEST_F(ReusableTraitsTest, message_repeated_reusable) {
  Reuse::AllocationMetadata<TestMessage> meta;
  auto& m = *allocator.create_object<TestMessage>();
  for (size_t i = 0; i < 186; ++i) {
    m.mutable_ri32()->Add(i);
    m.mutable_rs()->Add()->reserve(10010);
  }
  Reuse::reconstruct(m, allocator);
  ASSERT_TRUE(m.ri32().empty());
  ASSERT_LE(186, m.ri32().Capacity());
  ASSERT_TRUE(m.rs().empty());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  ASSERT_EQ(186, m.rs().ClearedCount());
#pragma GCC diagnostic pop
  Reuse::update_allocation_metadata(m, meta);
  {
    auto& mm =
        *Reuse::create_with_allocation_metadata<TestMessage>(allocator, meta);
    ASSERT_TRUE(mm.ri32().empty());
    ASSERT_LE(186, mm.ri32().Capacity());
    ASSERT_TRUE(mm.rs().empty());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    ASSERT_EQ(186, mm.rs().ClearedCount());
#pragma GCC diagnostic pop
    mm.add_rs("123");
    ASSERT_LE(10010, mm.rs(0).capacity());
  }
}

TEST_F(ReusableTraitsTest, message_message_reusable) {
  Reuse::AllocationMetadata<TestMessage> meta;
  auto& m = *allocator.create_object<TestMessage>();
  m.mutable_m()->mutable_s()->reserve(10086);
  for (size_t i = 0; i < 186; ++i) {
    m.mutable_rm()->Add()->mutable_s()->reserve(10086);
  }
  Reuse::reconstruct(m, allocator);
  ASSERT_TRUE(m.m().s().empty());
  ASSERT_LE(10086, m.m().s().capacity());
  ASSERT_TRUE(m.rm().empty());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  ASSERT_EQ(186, m.rm().ClearedCount());
#pragma GCC diagnostic pop
  Reuse::update_allocation_metadata(m, meta);
  {
    auto& mm =
        *Reuse::create_with_allocation_metadata<TestMessage>(allocator, meta);
    ASSERT_TRUE(mm.m().s().empty());
    ASSERT_LE(10086, mm.m().s().capacity());
    ASSERT_TRUE(mm.rm().empty());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    ASSERT_EQ(186, mm.rm().ClearedCount());
#pragma GCC diagnostic pop
    ASSERT_LE(10086, mm.mutable_rm()->Add()->s().capacity());
  }
}
*/
#endif // BABYLON_USE_PROTOBUF
