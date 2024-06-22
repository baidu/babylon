#include "babylon/reusable/memory_resource.h"

#if BABYLON_USE_PROTOBUF
#include <arena_example.pb.h>
#endif // BABYLON_USE_PROTOBUF

#include "gtest/gtest.h"

#include <thread>

using ::babylon::ExclusiveMonotonicBufferResource;
using ::babylon::MonotonicBufferResource;
using ::babylon::NewDeletePageAllocator;
using ::babylon::PageAllocator;
using ::babylon::PageHeap;
using ::babylon::SharedMonotonicBufferResource;
using ::babylon::SwissMemoryResource;
using ::babylon::SystemPageAllocator;
;

#if BABYLON_USE_PROTOBUF
using ::babylon::ArenaExample;
#endif // BABYLON_USE_PROTOBUF
using ::google::protobuf::Arena;

struct MockResource : public ::std::pmr::memory_resource {
  virtual void* do_allocate(size_t bytes, size_t alignment) noexcept override {
    allocate_called = true;
    return ::operator new(bytes, ::std::align_val_t(alignment));
  }

  virtual void do_deallocate(void* ptr, size_t bytes,
                             size_t alignment) noexcept override {
    deallocate_called = true;
    ::operator delete(ptr, bytes, ::std::align_val_t(alignment));
  }

  virtual bool do_is_equal(
      const memory_resource& __other) const noexcept override {
    return this == &__other;
  }

  bool allocate_called {false};
  bool deallocate_called {false};
};

TEST(ExclusiveMonotonicBufferResource, equal_only_when_same) {
  ExclusiveMonotonicBufferResource resource1;
  ExclusiveMonotonicBufferResource resource2;
  ASSERT_TRUE(resource1.is_equal(resource1));
  ASSERT_TRUE(resource1 == resource1);
  ASSERT_FALSE(resource1 != resource1);
  ASSERT_FALSE(resource1.is_equal(resource2));
  ASSERT_FALSE(resource1 == resource2);
  ASSERT_TRUE(resource1 != resource2);
}

TEST(ExclusiveMonotonicBufferResource, allocate_with_alignment) {
  ExclusiveMonotonicBufferResource resource;
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(resource.allocate(1, 32)) % 32);
  }
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(resource.allocate<64>(1)) % 64);
  }
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(0,
              reinterpret_cast<uintptr_t>(resource.allocate<8192>(1)) % 8192);
  }
}

TEST(ExclusiveMonotonicBufferResource, allocate_oversize_with_alignment) {
  ExclusiveMonotonicBufferResource resource;
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(resource.allocate(8000, 32)) % 32);
  }
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(0, reinterpret_cast<uintptr_t>(resource.allocate<64>(8000)) % 64);
  }
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(
        0, reinterpret_cast<uintptr_t>(resource.allocate<8192>(8000)) % 8192);
  }
}

TEST(ExclusiveMonotonicBufferResource, allocate_oversize_use_upstream) {
  MockResource upstream;
  ExclusiveMonotonicBufferResource resource;
  resource.set_upstream(upstream);

  auto page_size = resource.page_allocator().page_size();
  for (size_t i = 0; i < page_size; i += page_size / 1024) {
    resource.allocate(i, 1);
    resource.allocate(1, i);
  }
  ASSERT_FALSE(upstream.allocate_called);
  ASSERT_FALSE(upstream.deallocate_called);

  resource.allocate(page_size + 1, 1);
  ASSERT_TRUE(upstream.allocate_called);
  resource.release();
  ASSERT_TRUE(upstream.deallocate_called);

  upstream.allocate_called = false;
  upstream.deallocate_called = false;

  resource.allocate(1, ::absl::bit_ceil(page_size + 1));
  ASSERT_TRUE(upstream.allocate_called);
  resource.release();
  ASSERT_TRUE(upstream.deallocate_called);
}

TEST(ExclusiveMonotonicBufferResource, check_address_inside_resource) {
  ExclusiveMonotonicBufferResource resource;
  ::std::vector<void*> resource_allocated;
  ::std::vector<void*> heap_allocated;
  auto page_size = resource.page_allocator().page_size();
  for (size_t i = page_size - 16; i < page_size + 16; ++i) {
    auto ptr = resource.allocate(i, 1 << (i % 4));
    resource_allocated.emplace_back(ptr);
    heap_allocated.emplace_back(::operator new(i));
  }
  for (auto address : resource_allocated) {
    ASSERT_TRUE(resource.contains(address));
  }
  for (auto address : heap_allocated) {
    ASSERT_FALSE(resource.contains(address));
    ::operator delete(address);
  }
}

TEST(ExclusiveMonotonicBufferResource, moveable) {
  ExclusiveMonotonicBufferResource resource;
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(0, (size_t)resource.allocate(1, 32) % 32);
  }
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(0, (size_t)resource.allocate(8000, 32) % 32);
  }
  for (size_t i = 0; i < 10; ++i) {
    auto s = reinterpret_cast<::std::string*>(
        resource.allocate<alignof(::std::string)>(sizeof(::std::string)));
    new (s)::std::string("10086");
    resource.register_destructor(s);
  }
  ExclusiveMonotonicBufferResource other_resource(::std::move(resource));
  ExclusiveMonotonicBufferResource another_resource;
  another_resource = ::std::move(other_resource);
  another_resource.release();
}

TEST(ExclusiveMonotonicBufferResource, use_specific_page_heap) {
  PageHeap heap;
  ExclusiveMonotonicBufferResource resource;
  resource.set_page_allocator(heap);
  auto* ptr = resource.allocate(1, 32);
  resource.release();
  auto* page = heap.allocate();
  ASSERT_EQ(ptr, page);
  heap.deallocate(&page, 1);
}

TEST(ExclusiveMonotonicBufferResource,
     destructor_call_when_release_in_reverse_order) {
  struct S {
    ~S() {
      deleted().emplace_back(this);
    }
    static ::std::vector<S*>& deleted() {
      static ::std::vector<S*> vec;
      return vec;
    }
  };
  ExclusiveMonotonicBufferResource resource;
  for (size_t i = 0; i < 10; ++i) {
    resource.register_destructor(reinterpret_cast<S*>(i));
  }
  resource.release();
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(reinterpret_cast<S*>(9 - i), S::deleted()[i]);
  }
}

TEST(ExclusiveMonotonicBufferResource, can_work_with_pmr) {
  ExclusiveMonotonicBufferResource resource;
#if BABYLON_HAS_POLYMORPHIC_MEMORY_RESOURCE
  ::std::pmr::vector<size_t> v0(512, &resource);
  ::std::pmr::vector<size_t> v1(256, &resource);
  ::std::pmr::vector<size_t> v2(128, &resource);
  ASSERT_EQ(v1.data() + 256, v2.data());
#else
  ::std::pmr::memory_resource& std_resource = resource;
  std_resource.deallocate(std_resource.allocate(1, 32), 1, 32);
  std_resource.deallocate(std_resource.allocate(20, 64), 20, 64);
  std_resource.deallocate(std_resource.allocate(3000, 8192), 3000, 8192);
#endif
}

TEST(ExclusiveMonotonicBufferResource, can_work_with_monotonic) {
  ExclusiveMonotonicBufferResource resource;
  MonotonicBufferResource& mono_resource = resource;
  mono_resource.allocate(8, 32);
  auto ptr = (::std::string*)mono_resource.allocate<alignof(::std::string)>(
      sizeof(std::string));
  new (ptr)::std::string(1024, 'x');
  mono_resource.register_destructor(ptr);
}

TEST(SharedMonotonicBufferResource, allocate_oversize_use_upstream) {
  MockResource upstream;
  SharedMonotonicBufferResource resource;
  resource.set_upstream(upstream);

  auto page_size = resource.page_allocator().page_size();
  ::std::vector<::std::thread> threads;
  threads.reserve(4);
  for (size_t i = 0; i < 4; ++i) {
    threads.emplace_back([&] {
      for (size_t j = page_size; j < page_size; ++j) {
        resource.allocate(j, 1);
        resource.allocate(1, j);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  ASSERT_FALSE(upstream.allocate_called);
  ASSERT_FALSE(upstream.deallocate_called);

  resource.allocate(page_size + 1, 1);
  ASSERT_TRUE(upstream.allocate_called);
  resource.release();
  ASSERT_TRUE(upstream.deallocate_called);

  upstream.allocate_called = false;
  upstream.deallocate_called = false;

  resource.allocate(1, ::absl::bit_ceil(page_size + 1));
  ASSERT_TRUE(upstream.allocate_called);
  resource.release();
  ASSERT_TRUE(upstream.deallocate_called);
}

TEST(SwissMemoryResource, allocate_oversize_use_upstream) {
  MockResource upstream;
  SwissMemoryResource resource;
  resource.set_upstream(upstream);

  auto page_size = resource.page_allocator().page_size();
  ::std::vector<::std::thread> threads;
  threads.reserve(4);
  for (size_t i = 0; i < 4; ++i) {
    threads.emplace_back([&] {
      for (size_t j = page_size; j < page_size; ++j) {
        resource.allocate(j, 1);
        resource.allocate(1, j);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  ASSERT_FALSE(upstream.allocate_called);
  ASSERT_FALSE(upstream.deallocate_called);

  resource.allocate(page_size + 1, 1);
  ASSERT_TRUE(upstream.allocate_called);
  resource.release();
  ASSERT_TRUE(upstream.deallocate_called);

  upstream.allocate_called = false;
  upstream.deallocate_called = false;

  resource.allocate(1, ::absl::bit_ceil(page_size + 1));
  ASSERT_TRUE(upstream.allocate_called);
  resource.release();
  ASSERT_TRUE(upstream.deallocate_called);
}

TEST(SharedMonotonicBufferResource, check_address_inside_resource) {
  SharedMonotonicBufferResource resource;

  auto page_size = resource.page_allocator().page_size();
  ::std::vector<void*> resource_allocated(32);
  ::std::vector<void*> heap_allocated(32);
  ::std::vector<::std::thread> threads;
  threads.reserve(32);
  for (size_t i = 0; i < 32; ++i) {
    threads.emplace_back([&, i] {
      auto j = page_size - 16 + i;
      resource_allocated[i] = resource.allocate(j, 1 << (i % 4));
      heap_allocated[i] = ::operator new(j);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (auto address : resource_allocated) {
    ASSERT_TRUE(resource.contains(address));
  }
  for (auto address : heap_allocated) {
    ASSERT_FALSE(resource.contains(address));
    ::operator delete(address);
  }
}

TEST(SharedMonotonicBufferResource, allocate_thread_safe) {
  PageHeap page_heap;
  SharedMonotonicBufferResource resource;
  resource.set_page_allocator(page_heap);
  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < 10; ++i) {
    threads.emplace_back([&] {
      for (size_t j = 0; j < 1000; ++j) {
#if BABYLON_HAS_POLYMORPHIC_MEMORY_RESOURCE
        ::std::pmr::vector<size_t> vector(j % 512, &resource);
#endif
        auto* s = reinterpret_cast<::std::string*>(
            resource.allocate<alignof(::std::string)>(sizeof(::std::string)));
        new (s)::std::string(200, 'x');
        resource.register_destructor(s);
        s = reinterpret_cast<::std::string*>(
            resource.allocate<alignof(::std::string)>(sizeof(::std::string)));
        new (s)::std::string(20000, 'x');
        resource.register_destructor(s);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST(SwissMemoryResource, compatible_with_protobuf_in_asan_mode) {
  PageHeap page_heap;
  for (size_t i = 0; i < 10; ++i) {
    SwissMemoryResource resource(page_heap);
    for (int j = 0; j < 10; ++j) {
      Arena& arena = resource;
#if GOOGLE_PROTOBUF_VERSION >= 5026000
      auto message = Arena::Create<ArenaExample>(&arena);
#else  // GOOGLE_PROTOBUF_VERSION < 5026000
      auto message = Arena::CreateMessage<ArenaExample>(&arena);
#endif // GOOGLE_PROTOBUF_VERSION < 5026000
      // 反复reserve会触发protobuf 4.x的重用功能
      // 内部有对应的asan poison标记动作
      // 验证和MemoryResource的标记可兼容
      for (int i = 0; i < 10; ++i) {
        message->mutable_m()->add_rp(i);
      }
    }
  }
}

TEST(SharedMonotonicBufferResource, thread_exchange_safe) {
  struct S {};
  SharedMonotonicBufferResource resource;
  ::std::vector<::std::thread> threads;
  ::std::atomic<int> counter {0};
  ::std::vector<::std::string*> ptrs {10, nullptr};
  for (size_t i = 0; i < 10; ++i) {
    threads.emplace_back([&, i] {
      auto s = reinterpret_cast<::std::string*>(
          resource.allocate<alignof(::std::string)>(sizeof(::std::string)));
      new (s)::std::string(200, 'x');
      resource.register_destructor(s);
      ptrs[i] = s;
      counter.fetch_add(1);
      while (counter.load() != 10) {
        ::usleep(1000);
      }
      s = ptrs[(i + 1) % 10];
      s->resize(10000);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST(SharedMonotonicBufferResource, count_space_used_and_allocated) {
  NewDeletePageAllocator allocator;
  allocator.set_page_size(1024);

  SharedMonotonicBufferResource resource(allocator);
  ASSERT_EQ(0, resource.space_used());
  ASSERT_EQ(0, resource.space_allocated());

  resource.allocate<1>(1);
  ASSERT_EQ(1, resource.space_used());
  ASSERT_EQ(1024, resource.space_allocated());

  resource.allocate<1>(4);
  ASSERT_EQ(5, resource.space_used());
  ASSERT_EQ(1024, resource.space_allocated());

  resource.allocate<512>(512);
  ASSERT_EQ(517, resource.space_used());
  ASSERT_EQ(1024, resource.space_allocated());

  resource.allocate<1>(1);
  ASSERT_EQ(518, resource.space_used());
  ASSERT_EQ(2048, resource.space_allocated());

  resource.release();
  ASSERT_EQ(0, resource.space_used());
  ASSERT_EQ(0, resource.space_allocated());
}

TEST(memory_resource, construct_with_or_without_page_heap) {
  {
    SwissMemoryResource resource;
    ::memset(resource.allocate<64>(128), 0, 128);
  }
  {
    PageHeap page_heap;
    SwissMemoryResource resource;
    resource.set_page_allocator(page_heap);
    ::memset(resource.allocate<32>(1024), 0, 1024);
  }
}

#if BABYLON_USE_PROTOBUF
TEST(memory_resource, can_use_as_arena_with_protobuf) {
  SwissMemoryResource resource;
  resource.allocate<1>(128);
  auto* ptr_in_resource = (char*)resource.allocate<1>(128);
  Arena& arena = resource;
#if GOOGLE_PROTOBUF_VERSION >= 5026000
  auto message = Arena::Create<ArenaExample>(&arena);
#else  // GOOGLE_PROTOBUF_VERSION < 5026000
  auto message = Arena::CreateMessage<ArenaExample>(&arena);
#endif // GOOGLE_PROTOBUF_VERSION < 5026000
  for (size_t i = 0; i < 1024; ++i) {
    message->add_rs("10086");
    message->add_rp(10086);
  }
  ASSERT_EQ(&arena, message->GetArena());
  ASSERT_EQ(&arena, message->mutable_m()->GetArena());
  ASSERT_LE(ptr_in_resource + 128, (char*)message);
  ASSERT_GT(ptr_in_resource + 1024, (char*)message);
  ASSERT_EQ(0, arena.SpaceUsed());
}
#endif // BABYLON_USE_PROTOBUF

TEST(memory_resource, also_works_as_arena_with_non_protobuf) {
  struct S {
    S(const char* str) : s(str) {
      nums()++;
    }
    ~S() {
      nums()--;
    }
    static size_t& nums() {
      static size_t n = 0;
      return n;
    }
    ::std::string s;
  };
  {
    SwissMemoryResource resource;
    Arena& arena = resource;
    Arena::Create<S>(&arena, "10086");
    ASSERT_EQ(1, S::nums());
    ASSERT_EQ(0, arena.SpaceUsed());
  }
  ASSERT_EQ(0, S::nums());
}

TEST(memory_resource, also_works_as_arena_for_owning) {
  SwissMemoryResource resource;
  Arena& arena = resource;
  auto ptr = new ::std::string;
  ptr->resize(1024);
  arena.Own(ptr);
  ASSERT_EQ(0, arena.SpaceUsed());
}

#if BABYLON_USE_PROTOBUF
TEST(memory_resource, release_also_clear_arena) {
  SwissMemoryResource resource;
  {
    resource.allocate<1>(128);
    auto* ptr_in_resource = (char*)resource.allocate<1>(128);
    Arena& arena = resource;
#if GOOGLE_PROTOBUF_VERSION >= 5026000
    auto message = Arena::Create<ArenaExample>(&arena);
#else  // GOOGLE_PROTOBUF_VERSION < 5026000
    auto message = Arena::CreateMessage<ArenaExample>(&arena);
#endif // GOOGLE_PROTOBUF_VERSION < 5026000
    ASSERT_EQ(&arena, message->GetArena());
    ASSERT_EQ(&arena, message->mutable_m()->GetArena());
    ASSERT_LE(ptr_in_resource + 128, (char*)message);
    ASSERT_GT(ptr_in_resource + 1024, (char*)message);
    Arena::Create<::std::string>(&arena);
  }
  resource.release();
  {
    resource.allocate<1>(128);
    auto* ptr_in_resource = (char*)resource.allocate<1>(128);
    Arena& arena = resource;
#if GOOGLE_PROTOBUF_VERSION >= 5026000
    auto message = Arena::Create<ArenaExample>(&arena);
#else  // GOOGLE_PROTOBUF_VERSION < 5026000
    auto message = Arena::CreateMessage<ArenaExample>(&arena);
#endif // GOOGLE_PROTOBUF_VERSION < 5026000
    ASSERT_EQ(&arena, message->GetArena());
    ASSERT_EQ(&arena, message->mutable_m()->GetArena());
    ASSERT_LE(ptr_in_resource + 128, (char*)message);
    ASSERT_GT(ptr_in_resource + 1024, (char*)message);
    Arena::Create<::std::string>(&arena);
  }
}
#endif // BABYLON_USE_PROTOBUF
