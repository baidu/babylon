#include "babylon/reusable/allocator.h"

#if BABYLON_USE_PROTOBUF
#include <arena_example.pb.h>
#endif // BABYLON_USE_PROTOBUF

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"

#include <vector>

#if BABYLON_USE_PROTOBUF
using ::babylon::ArenaExample;
#endif // BABYLON_USE_PROTOBUF
using ::babylon::ExclusiveMonotonicBufferResource;
using ::babylon::MonotonicAllocator;
using ::babylon::UsesAllocatorConstructor;
template <typename... Args>
using Constructible = UsesAllocatorConstructor::Constructible<Args...>;
using ::babylon::SharedMonotonicBufferResource;
using ::babylon::SwissAllocator;
using ::babylon::SwissMemoryResource;

using ::google::protobuf::Arena;

TEST(UsesAllocatorConstructor, constructible_with_raw_args) {
  struct S {
    S() = default;
    S(int) {
      v = 1;
    }
    int v {0};
  } s;
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int>::value));
  ASSERT_FALSE((Constructible<S, ::std::allocator<void>, int>::USES_ALLOCATOR));
  ASSERT_FALSE((Constructible<S, ::std::allocator<void>, int, int>::value));
  ASSERT_FALSE(
      (Constructible<S, ::std::allocator<void>, int, int>::USES_ALLOCATOR));
  s.~S();
  UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(), 2);
  ASSERT_EQ(1, s.v);
}

TEST(UsesAllocatorConstructor, constructible_with_allocator) {
  struct S {
    typedef ::std::allocator<int> allocator_type;
    S() = default;
    S(int) {
      v = 1;
    }
    S(::std::allocator_arg_t, allocator_type, int) {
      v = 2;
    }
    S(int, int) {
      v = 3;
    }
    S(int, int, allocator_type) {
      v = 4;
    }
    S(int, int, int) {
      v = 5;
    }
    int v {0};
  } s;
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int>::value));
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int>::USES_ALLOCATOR));
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int, int>::value));
  ASSERT_TRUE(
      (Constructible<S, ::std::allocator<void>, int, int>::USES_ALLOCATOR));
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int, int, int>::value));
  ASSERT_FALSE((
      Constructible<S, ::std::allocator<void>, int, int, int>::USES_ALLOCATOR));
  s.~S();
  UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(), 2);
  ASSERT_EQ(2, s.v);
  s.~S();
  UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(), 2, 3);
  ASSERT_EQ(4, s.v);
  s.~S();
  UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(), 2, 3, 4);
  ASSERT_EQ(5, s.v);
}

TEST(UsesAllocatorConstructor, check_standard_uses_allocator_first) {
  struct S {
    typedef ::std::allocator<int> A;
    S() = default;
    S(int) {
      v = 1;
    }
    S(::std::allocator_arg_t, A, int) {
      v = 2;
    }
    S(int, int) {
      v = 3;
    }
    S(int, int, A) {
      v = 4;
    }
    S(int, int, int) {
      v = 5;
    }
    int v {0};
  } s;
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int>::value));
  ASSERT_FALSE((Constructible<S, ::std::allocator<void>, int>::USES_ALLOCATOR));
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int, int>::value));
  ASSERT_FALSE(
      (Constructible<S, ::std::allocator<void>, int, int>::USES_ALLOCATOR));
  ASSERT_TRUE((Constructible<S, ::std::allocator<void>, int, int, int>::value));
  ASSERT_FALSE((
      Constructible<S, ::std::allocator<void>, int, int, int>::USES_ALLOCATOR));
  s.~S();
  UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(), 2);
  ASSERT_EQ(1, s.v);
  s.~S();
  UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(), 2, 3);
  ASSERT_EQ(3, s.v);
  s.~S();
  UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(), 2, 3, 4);
  ASSERT_EQ(5, s.v);
}

TEST(UsesAllocatorConstructor, check_already_has_allocator_in_args) {
  {
    struct S {
      typedef ::std::allocator<int> allocator_type;
      S() = default;
      S(::std::allocator_arg_t, allocator_type) {
        v = 1;
      }
      S(::std::allocator_arg_t, allocator_type, ::std::allocator_arg_t,
        allocator_type) {
        v = 2;
      }
      int v {0};
    } s;
    ASSERT_TRUE((Constructible<S, ::std::allocator<void>>::value));
    ASSERT_TRUE((Constructible<S, ::std::allocator<void>>::USES_ALLOCATOR));
    ASSERT_TRUE(
        (Constructible<S, ::std::allocator<void>, ::std::allocator_arg_t,
                       ::std::allocator<int>>::value));
    ASSERT_FALSE(
        (Constructible<S, ::std::allocator<void>, ::std::allocator_arg_t,
                       ::std::allocator<int>>::USES_ALLOCATOR));
    s.~S();
    UsesAllocatorConstructor::construct(&s, ::std::allocator<void>());
    ASSERT_EQ(1, s.v);
    s.~S();
    UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(),
                                        ::std::allocator_arg,
                                        ::std::allocator<int>());
    ASSERT_EQ(1, s.v);
    s.~S();
    UsesAllocatorConstructor::construct(
        &s, ::std::allocator<void>(), ::std::allocator_arg,
        ::std::allocator<int>(), ::std::allocator_arg, ::std::allocator<int>());
    ASSERT_EQ(2, s.v);
  }
  {
    struct S {
      typedef ::std::allocator<int> allocator_type;
      S() = default;
      S(allocator_type) {
        v = 1;
      }
      S(allocator_type, allocator_type) {
        v = 2;
      }
      int v {0};
    } s;
    ASSERT_TRUE((Constructible<S, ::std::allocator<void>>::value));
    ASSERT_TRUE((Constructible<S, ::std::allocator<void>>::USES_ALLOCATOR));
    ASSERT_TRUE((Constructible<S, ::std::allocator<void>,
                               ::std::allocator<int>>::value));
    ASSERT_FALSE((Constructible<S, ::std::allocator<void>,
                                ::std::allocator<int>>::USES_ALLOCATOR));
    s.~S();
    UsesAllocatorConstructor::construct(&s, ::std::allocator<void>());
    ASSERT_EQ(1, s.v);
    s.~S();
    UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(),
                                        ::std::allocator<int>());
    ASSERT_EQ(1, s.v);
    s.~S();
    UsesAllocatorConstructor::construct(&s, ::std::allocator<void>(),
                                        ::std::allocator<int>(),
                                        ::std::allocator<int>());
    ASSERT_EQ(2, s.v);
  }
}

TEST(UsesAllocatorConstructor,
     support_pair_even_cant_pass_uses_allocator_test) {
  struct S {
    typedef ::std::allocator<int> allocator_type;
    S() = default;
    S(allocator_type) {
      v = 1;
    }
    S(int) {
      v = 2;
    }
    S(int, allocator_type) {
      v = 3;
    }
    S(void*) {
      v = 4;
    }
    int v {0};
  };
  ASSERT_FALSE((::std::uses_allocator<::std::pair<S, int>,
                                      ::std::allocator<void>>::value));
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>, int,
                             int>::value));
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>, int,
                             int>::USES_ALLOCATOR));
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>, void*,
                             int>::value));
  ASSERT_FALSE((Constructible<::std::pair<S, int>, ::std::allocator<void>,
                              void*, int>::USES_ALLOCATOR));
  {
    ::std::pair<S, int> p;
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>());
    ASSERT_EQ(1, p.first.v);
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(), 1, 1);
    ASSERT_EQ(3, p.first.v);
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(), nullptr,
                                        1);
    ASSERT_EQ(4, p.first.v);
  }
  {
    ::std::pair<int, S> p;
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>());
    ASSERT_EQ(1, p.second.v);
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(), 1, 1);
    ASSERT_EQ(3, p.second.v);
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(), 1,
                                        nullptr);
    ASSERT_EQ(4, p.second.v);
  }
  {
    ::std::pair<S, S> p;
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>());
    ASSERT_EQ(1, p.first.v);
    ASSERT_EQ(1, p.second.v);
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(), 1, 1);
    ASSERT_EQ(3, p.first.v);
    ASSERT_EQ(3, p.second.v);
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(), nullptr,
                                        nullptr);
    ASSERT_EQ(4, p.first.v);
    ASSERT_EQ(4, p.second.v);
  }
}

TEST(UsesAllocatorConstructor, support_pair_copy_and_move) {
  struct S {
    typedef ::std::allocator<int> allocator_type;
    S() = default;
    S(S&&) {
      v = 1;
    }
    S(const S&) {
      v = 2;
    }
    S(S&&, allocator_type) {
      v = 3;
    }
    S(const S&, allocator_type) {
      v = 4;
    }
    int v {0};
  };
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>,
                             ::std::pair<S, int>>::value));
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>,
                             ::std::pair<S, int>>::USES_ALLOCATOR));
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>,
                             const ::std::pair<S, int>&>::value));
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>,
                             const ::std::pair<S, int>&>::USES_ALLOCATOR));
  {
    ::std::pair<S, int> pp;
    ::std::pair<S, int> p;
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(),
                                        ::std::move(pp));
    ASSERT_EQ(3, p.first.v);
    p.~pair();
    UsesAllocatorConstructor::construct(&p, ::std::allocator<void>(), pp);
    ASSERT_EQ(4, p.first.v);
  }
}

TEST(UsesAllocatorConstructor, support_pair_piecewise_construct) {
  struct S {
    typedef ::std::allocator<int> allocator_type;
    S() = default;
    S(int) {
      v = 1;
    }
    S(int, int) {
      v = 2;
    }
    S(int, int, allocator_type) {
      v = 3;
    }
    int v {0};
  };
  ASSERT_TRUE((Constructible<::std::pair<S, int>, ::std::allocator<void>,
                             ::std::piecewise_construct_t, ::std::tuple<int>,
                             ::std::tuple<int>>::value));
  ASSERT_FALSE((Constructible<::std::pair<S, int>, ::std::allocator<void>,
                              ::std::piecewise_construct_t, ::std::tuple<int>,
                              ::std::tuple<int>>::USES_ALLOCATOR));
  ASSERT_TRUE(
      (Constructible<::std::pair<S, int>, ::std::allocator<void>,
                     ::std::piecewise_construct_t, ::std::tuple<int, int>,
                     ::std::tuple<int>>::value));
  ASSERT_TRUE(
      (Constructible<::std::pair<S, int>, ::std::allocator<void>,
                     ::std::piecewise_construct_t, ::std::tuple<int, int>,
                     ::std::tuple<int>>::USES_ALLOCATOR));
  {
    ::std::pair<S, int> p;
    p.~pair();
    UsesAllocatorConstructor::construct(
        &p, ::std::allocator<void>(), ::std::piecewise_construct,
        ::std::tuple<int>(1), ::std::tuple<int>(1));
    ASSERT_EQ(1, p.first.v);
    p.~pair();
    UsesAllocatorConstructor::construct(
        &p, ::std::allocator<void>(), ::std::piecewise_construct,
        ::std::tuple<int, int>(1, 1), ::std::tuple<int>(1));
    ASSERT_EQ(3, p.first.v);
  }
}

TEST(MonotonicAllocator, construct_from_monotonic_memory_resource) {
  {
    ExclusiveMonotonicBufferResource resource;
    auto* bytes =
        MonotonicAllocator<void, ExclusiveMonotonicBufferResource>(resource)
            .allocate_bytes(128, 64);
    ::memset(bytes, 0, 128);
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(bytes) % 64);
    bytes = MonotonicAllocator<>(resource).allocate_bytes(128, 64);
    ::memset(bytes, 0, 128);
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(bytes) % 64);
  }
  {
    SharedMonotonicBufferResource resource;
    auto* bytes =
        MonotonicAllocator<void, SharedMonotonicBufferResource>(resource)
            .allocate_bytes(128, 64);
    ::memset(bytes, 0, 128);
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(bytes) % 64);
    bytes = MonotonicAllocator<>(resource).allocate_bytes(128, 64);
    ::memset(bytes, 0, 128);
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(bytes) % 64);
  }
}

TEST(MonotonicAllocator, allocate_aligned_object) {
  struct alignas(64) S {};
  ExclusiveMonotonicBufferResource resource;
  for (size_t i = 0; i < 64; ++i) {
    typedef MonotonicAllocator<S, ExclusiveMonotonicBufferResource> A;
    A(resource).allocate_bytes(i, 1);
    auto address = reinterpret_cast<uintptr_t>(A(resource).allocate(1));
    ASSERT_EQ(0, address % 64);
  }
  for (size_t i = 0; i < 64; ++i) {
    typedef MonotonicAllocator<S> A;
    A(resource).allocate_bytes(i, 1);
    auto address = reinterpret_cast<uintptr_t>(A(resource).allocate(1));
    ASSERT_EQ(0, address % 64);
  }
  for (size_t i = 0; i < 64; ++i) {
    typedef MonotonicAllocator<S> A;
    A(resource).allocate_bytes(i, 1);
    auto address =
        reinterpret_cast<uintptr_t>(A(resource).allocate_object<S>());
    ASSERT_EQ(0, address % 64);
  }
  for (size_t i = 0; i < 64; ++i) {
    typedef MonotonicAllocator<S> A;
    A(resource).allocate_bytes(i, 1);
    auto* object = A(resource).new_object<S>();
    auto address = reinterpret_cast<uintptr_t>(object);
    ASSERT_EQ(0, address % 64);
    A(resource).delete_object(object);
  }
  for (size_t i = 0; i < 64; ++i) {
    typedef MonotonicAllocator<S> A;
    A(resource).allocate_bytes(i, 1);
    auto address = reinterpret_cast<uintptr_t>(A(resource).create_object<S>());
    ASSERT_EQ(0, address % 64);
  }
}

TEST(MonotonicAllocator, support_uses_allocator_construct) {
  struct S {
    using allocator_type = MonotonicAllocator<>;
    S() = default;
    S(int, allocator_type) {
      v = 1;
    }
    int v {0};
  };
  SharedMonotonicBufferResource resource;
  typedef MonotonicAllocator<S> A;
  auto s = A(resource).create();
  ASSERT_EQ(0, s->v);
  s = A(resource).create(2);
  ASSERT_EQ(1, s->v);
  typedef ::std::vector<S, MonotonicAllocator<S>> V;
  auto v = A(resource).create_object<V>();
  v->emplace_back();
  ASSERT_EQ(0, v->back().v);
  v->emplace_back(3);
  ASSERT_EQ(1, v->back().v);
}

TEST(MonotonicAllocator, register_destructor_and_call_when_resource_release) {
  struct S {
    S() = default;
    ~S() {
      ++destruct_times();
    }
    static int& destruct_times() {
      static int v;
      return v;
    }
  };
  ExclusiveMonotonicBufferResource resource;
  typedef MonotonicAllocator<S> A;
  S::destruct_times() = 0;
  A(resource).create();
  ASSERT_EQ(0, S::destruct_times());
  resource.release();
  ASSERT_EQ(1, S::destruct_times());
}

#if __cpp_lib_memory_resource >= 201603L
TEST(MonotonicAllocator, support_pmr) {
  SwissMemoryResource resource;
  typedef ::std::pmr::vector<::std::pmr::vector<char>> V;
  auto& v = *MonotonicAllocator<>(resource).create_object<V>();
  v.emplace_back(std::initializer_list<char> {'1', '0', '0', '8', '6'});
  ASSERT_TRUE(resource.contains(&v));
  ASSERT_TRUE(resource.contains(&v[0]));
  ASSERT_TRUE(resource.contains(&v[0][0]));
}

TEST(MonotonicAllocator, support_absl_pmr) {
  SwissMemoryResource resource;
  using A = ::std::pmr::polymorphic_allocator<
      ::std::pair<const ::std::pmr::string, ::std::pmr::string>>;
  using M = ::absl::flat_hash_map<
      ::std::pmr::string, ::std::pmr::string,
      ::absl::container_internal::hash_default_hash<::std::string>,
      ::absl::container_internal::hash_default_eq<::std::string>, A>;
  auto& m = *MonotonicAllocator<>(resource).create_object<M>();
  m.emplace("key",
            "this is a "
            "veryyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
            "yyyyyyyyyyyyyy long string");
  ASSERT_TRUE(resource.contains(&m));
  ASSERT_TRUE(resource.contains(&m["key"]));
  ASSERT_TRUE(resource.contains(m["key"].data()));
}
#endif // __cpp_lib_memory_resource >= 201603L

TEST(SwissAllocator, support_plain_struct) {
  struct S {
    S() {}
    S(const char* str) : s(str) {}
    ::std::string s;
  };
  SwissMemoryResource resource;
  SwissAllocator<S>(resource).create("10086");
  SwissAllocator<S>(resource).create();
}

TEST(SwissAllocator, support_stl_container) {
  SwissMemoryResource resource;
  ::std::vector<size_t, SwissAllocator<size_t>> vector(resource);
  vector.resize(128);
  ASSERT_EQ(&resource, vector.get_allocator().resource());
}

// TODO(lijiang01): 通过特判解决clang下is_constructible的问题
// clang在处理is_constructible时
// 如果匹配的构造函数包含默认参数，且默认参数无法默认构造
// 会引起编译错误而非设置value == false
#ifndef __clang__
TEST(SwissAllocator, support_uses_allocator) {
  SwissMemoryResource resource;
  auto* vector =
      SwissAllocator<>(resource)
          .create_object<::std::vector<size_t, SwissAllocator<size_t>>>(10);
  vector->resize(128);
}
#endif

#if BABYLON_USE_PROTOBUF
TEST(SwissAllocator, support_protobuf) {
  SwissMemoryResource resource;
  Arena& arena = resource;
  auto* m = SwissAllocator<ArenaExample>(resource).create();
  ASSERT_EQ(&arena, m->GetArena());
  auto* mm =
      SwissAllocator<>(resource).create_object<ArenaExample>(::std::move(*m));
  ASSERT_EQ(&arena, mm->GetArena());
  auto* mmm = SwissAllocator<>(resource).create_object<ArenaExample>(*mm);
  ASSERT_EQ(&arena, mmm->GetArena());
}

TEST(SwissAllocator, copy_from_repeated_protobuf) {
  ArenaExample message;
  for (int i = 0; i < 10; ++i) {
    auto rm = message.add_rm();
    rm->mutable_m()->set_p(i);
  }
  SwissMemoryResource resource;
  auto* m = SwissAllocator<ArenaExample>(resource).create();
  m->set_p(5);
  ASSERT_EQ(m->p(), 5);

  m->Swap(message.mutable_rm(1));
  ASSERT_EQ(m->m().p(), 1);

  m->CopyFrom(message.rm(2));
  ASSERT_EQ(m->m().p(), 2);

  m->set_p(1);
  ASSERT_EQ(m->p(), 1);
}
#endif // BABYLON_USE_PROTOBUF
