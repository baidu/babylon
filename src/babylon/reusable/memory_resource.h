#pragma once

#include "babylon/concurrent/counter.h"      // ConcurrentAdder
#include "babylon/concurrent/thread_local.h" // EnumerableThreadLocal
#include "babylon/environment.h"
#include "babylon/reusable/page_allocator.h" // PageAllocator
#include "babylon/sanitizer_helper.h"        // SanitizerHelper

#include "google/protobuf/stubs/common.h" // GOOGLE_PROTOBUF_VERSION

#if GOOGLE_PROTOBUF_VERSION >= 3000000
#include "google/protobuf/arena.h"
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

// clang-format off
#include "babylon/protect.h"
// clang-format on

#if __cpp_lib_memory_resource >= 201603L
#include <memory_resource>
#else // __cpp_lib_memory_resource < 201603L
namespace std {
namespace pmr {
// 对不支持memory_resource的情况，定义一个同样的接口实现，便于统一编码
#define BABYLON_TMP_DECLATE_MEMORY_RESOURCE class memory_resource {
BABYLON_TMP_DECLATE_MEMORY_RESOURCE
#undef BABYLON_TMP_DECLATE_MEMORY_RESOURCE
public:
memory_resource() noexcept = default;
memory_resource(memory_resource&&) noexcept = default;
memory_resource(const memory_resource&) noexcept = default;
memory_resource& operator=(memory_resource&&) noexcept = default;
memory_resource& operator=(const memory_resource&) noexcept = default;
virtual ~memory_resource() = 0;

inline void* allocate(size_t bytes,
                      size_t alignment = alignof(max_align_t)) noexcept;
inline void deallocate(void* ptr, size_t bytes,
                       size_t alignment = alignof(max_align_t)) noexcept;
inline bool is_equal(const memory_resource& other) const noexcept;

protected:
virtual void* do_allocate(size_t bytes, size_t alignment) = 0;
virtual void do_deallocate(void* ptr, size_t bytes, size_t alignment) = 0;
virtual bool do_is_equal(const memory_resource& other) const noexcept = 0;
}; // namespace pmr

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void* memory_resource::allocate(
    size_t bytes, size_t alignment) noexcept {
  return do_allocate(bytes, alignment);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void memory_resource::deallocate(
    void* ptr, size_t bytes, size_t alignment) noexcept {
  return do_deallocate(ptr, bytes, alignment);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool memory_resource::is_equal(
    const memory_resource& other) const noexcept {
  return do_is_equal(other);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator==(
    const memory_resource& left, const memory_resource& right) noexcept {
  return &left == &right || left.is_equal(right);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator!=(
    const memory_resource& left, const memory_resource& right) noexcept {
  return !(left == right);
}

memory_resource* new_delete_resource() noexcept;

} // namespace std
} // std
#endif // __cpp_lib_memory_resource < 201603L

BABYLON_NAMESPACE_BEGIN

// 类似std::pmr::monotonic_buffer_resource，但设计为不包含具体实现的接口类
// 其中deallocate固化为空操作，用来支持零散申请，统一释放的使用模式
class MonotonicBufferResource : public ::std::pmr::memory_resource {
 public:
  MonotonicBufferResource() noexcept = default;
  MonotonicBufferResource(MonotonicBufferResource&&) noexcept = default;
  MonotonicBufferResource(const MonotonicBufferResource&) = delete;
  MonotonicBufferResource& operator=(MonotonicBufferResource&&) noexcept =
      default;
  MonotonicBufferResource& operator=(const MonotonicBufferResource&) = delete;

  // 相比标准memory_resource，增加利用模板参数表达alignment的接口
  // 子类可以覆盖这个签名的函数，来实现针对常量alignment情况的加速
  template <size_t alignment>
  inline void* allocate(size_t bytes) noexcept;
  inline void* allocate(size_t bytes, size_t alignment) noexcept;

  // 提供托管析构的能力，类似google::protobuf::Arena::OwnDestructor
  // 主要用于支持MonotonicAllocator和ReusableManager实现生命周期托管
  template <typename T>
  inline void register_destructor(T* ptr) noexcept;
  inline void register_destructor(void* ptr,
                                  void (*destructor)(void*)) noexcept;

  // 统一释放功能，完成之前注册析构的执行，并释放所有已分配的内存
  virtual void release() noexcept = 0;

  ////////////////////////////////////////////////////////////////////////////
  // 辅助类函数，用于支持非核心功能，默认提供空实现，子类并不强制实现
  //
  // 判断ptr是否位于MonotonicBufferResource分配范围内
  virtual bool contains(const void* ptr) noexcept;
  //
  // 返回通过allocate分配返回的总量
  virtual size_t space_used() const noexcept;
  //
  // 返回支持allocate/register_destructor功能向底层申请的总量
  virtual size_t space_allocated() const noexcept;
  ////////////////////////////////////////////////////////////////////////////

 protected:
  template <typename T>
  static void destruct(void* ptr) noexcept;

  virtual void* do_allocate(size_t bytes,
                            size_t alignment) noexcept override = 0;
  virtual void do_deallocate(void* ptr, size_t bytes,
                             size_t alignment) noexcept override final;
  virtual bool do_is_equal(
      const ::std::pmr::memory_resource& other) const noexcept override;
  virtual void do_register_destructor(void* ptr,
                                      void (*destructor)(void*)) noexcept = 0;
};

// 独占使用的MonotonicBufferResource
// 分配操作线程不安全，但相对地性能也更好
class ExclusiveMonotonicBufferResource : public MonotonicBufferResource {
 public:
  // type erase方式存储的一个析构动作destructor(ptr)
  // ABI和google::protobuf::Arena一致，方便一些版本的protobuf自适配
  struct DestroyTask {
    void* ptr;
    void (*destructor)(void*);
  };

  static size_t allocate_oversize_page_num() noexcept;

  ExclusiveMonotonicBufferResource() noexcept = default;
  ExclusiveMonotonicBufferResource(ExclusiveMonotonicBufferResource&&) noexcept;
  ExclusiveMonotonicBufferResource& operator=(
      ExclusiveMonotonicBufferResource&&) noexcept;
  virtual ~ExclusiveMonotonicBufferResource() noexcept override;

  // 设置上游的内存分配器，分两种模式
  // 1、对于零散的小内存申请，聚集成页后向page_allocator申请
  // 2、对于大块的内存，直接转向upstream申请
  void set_page_allocator(PageAllocator& page_allocator) noexcept;
  void set_upstream(::std::pmr::memory_resource& upstream) noexcept;

  template <size_t alignment>
  inline void* allocate(size_t bytes) noexcept;
  inline void* allocate(size_t bytes, size_t alignment) noexcept;

  template <typename T>
  inline void register_destructor(T* ptr) noexcept;
  inline void register_destructor(void* ptr,
                                  void (*destructor)(void*)) noexcept;
  inline DestroyTask* get_destroy_task() noexcept;

  virtual void release() noexcept override;

  inline PageAllocator& page_allocator() noexcept;

  void destruct_all() noexcept;

  virtual bool contains(const void* ptr) noexcept override;
  virtual size_t space_used() const noexcept override;
  virtual size_t space_allocated() const noexcept override;

 protected:
  virtual void* do_allocate(size_t bytes, size_t alignment) noexcept override;
  virtual void do_register_destructor(
      void* ptr, void (*destructor)(void*)) noexcept override;

 private:
  static constexpr size_t PAGE_ARRAY_CAPACITY = 15;
  static constexpr size_t DESTROY_TASK_ARRAY_CAPACITY = 15;

  struct DestroyTaskArray {
    DestroyTaskArray* next;
    DestroyTask tasks[DESTROY_TASK_ARRAY_CAPACITY];
  };

  struct PageArray {
    PageArray* next;
    char* pages[PAGE_ARRAY_CAPACITY];
  };

  struct OversizePage {
    char* page;
    size_t bytes;
    size_t alignment;
  };

  struct OversizePageArray {
    OversizePageArray* next;
    OversizePage pages[PAGE_ARRAY_CAPACITY];
  };

  static ConcurrentAdder& oversize_page_concurrent_adder() noexcept;

  template <size_t alignment>
  inline void do_align() noexcept;
  inline void do_align(size_t alignment) noexcept;
  inline void* do_allocate_already_aligned(size_t bytes,
                                           size_t alignment) noexcept;
  void* do_allocate_in_new_page(size_t bytes, size_t alignment) noexcept;
  void* do_allocate_with_page_in_new_page_array(size_t bytes,
                                                char* page) noexcept;
  void* do_allocate_in_oversize_page(size_t bytes, size_t alignment) noexcept;

  DestroyTask* do_get_destroy_task_in_new_array() noexcept;

  PageAllocator* _page_allocator {&SystemPageAllocator::instance()};
  ::std::pmr::memory_resource* _upstream {::std::pmr::new_delete_resource()};

  PageArray* _last_page_array {nullptr};
  char** _last_page_pointer {_last_page_array->pages};
  char* _free_begin {nullptr};
  char* _free_end {nullptr};
  size_t _space_used {0};
  size_t _space_allocated {0};

  OversizePageArray* _last_oversize_page_array {nullptr};
  OversizePage* _last_oversize_page_pointer {_last_oversize_page_array->pages};

  DestroyTaskArray* _last_destroy_task_array {nullptr};
  DestroyTask* _last_destroy_task_pointer {_last_destroy_task_array->tasks};
};

// 可共享的MonotonicBufferResource
// 通过多个线程局部的ExclusiveMonotonicBufferResource实现
class SharedMonotonicBufferResource : public MonotonicBufferResource {
 public:
  using DestroyTask = ExclusiveMonotonicBufferResource::DestroyTask;

  SharedMonotonicBufferResource() noexcept = default;
  SharedMonotonicBufferResource(
      SharedMonotonicBufferResource&& other) noexcept = default;
  SharedMonotonicBufferResource& operator=(
      SharedMonotonicBufferResource&& other) noexcept = default;
  virtual ~SharedMonotonicBufferResource() noexcept override;

  // 设置上游的内存分配器，分两种模式
  // 1、对于零散的小内存申请，聚集成页后向page_allocator申请
  // 2、对于大块的内存，直接转向upstream申请
  SharedMonotonicBufferResource(PageAllocator& page_allocator) noexcept;
  void set_page_allocator(PageAllocator& page_allocator) noexcept;
  void set_upstream(::std::pmr::memory_resource& upstream) noexcept;

  template <size_t alignment>
  inline void* allocate(size_t bytes) noexcept;
  template <size_t alignment>
  void* allocate_slow(size_t bytes) noexcept;
  inline void* allocate(size_t bytes, size_t alignment) noexcept;

  template <typename T>
  inline void register_destructor(T* ptr) noexcept;
  inline void register_destructor(void* ptr,
                                  void (*destructor)(void*)) noexcept;
  inline DestroyTask* get_destroy_task() noexcept;

  virtual void release() noexcept override;

  inline PageAllocator& page_allocator() noexcept;

  virtual bool contains(const void* ptr) noexcept override;
  virtual size_t space_used() const noexcept override;
  virtual size_t space_allocated() const noexcept override;

 protected:
  virtual void* do_allocate(size_t bytes, size_t alignment) noexcept override;
  virtual void do_register_destructor(
      void* ptr, void (*destructor)(void*)) noexcept override;

 private:
  void register_thread_constructor() noexcept;

  PageAllocator* _page_allocator {&SystemPageAllocator::instance()};
  ::std::pmr::memory_resource* _upstream {::std::pmr::new_delete_resource()};
  EnumerableThreadLocal<ExclusiveMonotonicBufferResource> _resources;
};

// 多功能内存资源，除了通过std::memory_resource方式呈现外
// 额外支持以google::protobuf::Arena等其他方式呈现的内存资源
// 便于统一支持stl/protobuf等上层容器
class SwissMemoryResource : public SharedMonotonicBufferResource {
 public:
  inline SwissMemoryResource() noexcept
      : SwissMemoryResource(SystemPageAllocator::instance()) {}
  inline SwissMemoryResource(SwissMemoryResource&& other) noexcept
      : SharedMonotonicBufferResource {::std::move(other)},
        _arena {other._arena.load(::std::memory_order_relaxed)} {
    other._arena.store(nullptr, ::std::memory_order_relaxed);
  }
  inline SwissMemoryResource& operator=(SwissMemoryResource&& other) noexcept {
    _arena.store(other._arena.load(::std::memory_order_relaxed),
                 ::std::memory_order_relaxed);
    other._arena.store(nullptr, ::std::memory_order_relaxed);
    return *this;
  }

  inline SwissMemoryResource(PageAllocator& page_allocator) noexcept
      : SharedMonotonicBufferResource(page_allocator) {}

  void set_page_allocator(PageAllocator& page_allocator) noexcept;
  void set_upstream(::std::pmr::memory_resource& upstream) noexcept;

  virtual void release() noexcept override;

#if GOOGLE_PROTOBUF_VERSION >= 3000000
  inline operator ::google::protobuf::Arena&() noexcept {
    auto arena = _arena.load(::std::memory_order_acquire);
    if (ABSL_PREDICT_TRUE(arena)) {
      return *arena;
    }

    using Arena = ::google::protobuf::Arena;
    Arena* expected = nullptr;
    auto new_arena =
        reinterpret_cast<Arena*>(allocate<alignof(Arena)>(sizeof(Arena)));
    new (new_arena) Arena {make_arena_options()};
    if (!_arena.compare_exchange_strong(expected, new_arena,
                                        ::std::memory_order_acq_rel)) {
      return *expected;
    }
    return *new_arena;
  }
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

 private:
  // 用来注册给ArenaOption的分配/释放函数，本身的功能采用new/delete实现
  // 针对拦截hack未能生效的版本，能降级回原始的Arena实现
  static void* arena_block_alloc(size_t size) noexcept;
  static void arena_block_dealloc(void* ptr, size_t size) noexcept;

#if GOOGLE_PROTOBUF_VERSION >= 3000000
  inline ::google::protobuf::ArenaOptions make_arena_options() noexcept {
    ::google::protobuf::ArenaOptions options;
    // 在initial_block为nullptr时，注入initial_block_size是一个无效参数
    // 在拦截函数中通过识别block_alloc
    // 是否为SwissMemoryResource::arena_block_alloc
    // 就可以准确判断是否隶属于SwissMemoryResource
    // 并利用注入的this指针执行特殊操作
    options.initial_block = nullptr;
    options.initial_block_size = reinterpret_cast<size_t>(this);
    options.block_alloc = arena_block_alloc;
    options.block_dealloc = arena_block_dealloc;
    return options;
  }

  ::std::atomic<::google::protobuf::Arena*> _arena {nullptr};
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000
};

////////////////////////////////////////////////////////////////////////////////
// MonotonicBufferResource begin
template <size_t alignment>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void* MonotonicBufferResource::allocate(
    size_t bytes) noexcept {
  return do_allocate(bytes, alignment);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void* MonotonicBufferResource::allocate(
    size_t bytes, size_t alignment) noexcept {
  return do_allocate(bytes, alignment);
}

template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
MonotonicBufferResource::register_destructor(T* ptr) noexcept {
  if (!::std::is_trivially_destructible<T>::value) {
    register_destructor(ptr, &destruct<T>);
  }
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
MonotonicBufferResource::register_destructor(
    void* ptr, void (*destructor)(void*)) noexcept {
  do_register_destructor(ptr, destructor);
}

template <typename T>
void MonotonicBufferResource::destruct(void* ptr) noexcept {
  reinterpret_cast<T*>(ptr)->~T();
}
// MonotonicBufferResource end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ExclusiveMonotonicBufferResource begin
template <size_t alignment>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void*
ExclusiveMonotonicBufferResource::allocate(size_t bytes) noexcept {
  do_align<alignment>();
  return SanitizerHelper::unpoison(
      do_allocate_already_aligned(bytes, alignment), bytes);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void*
ExclusiveMonotonicBufferResource::allocate(size_t bytes,
                                           size_t alignment) noexcept {
  do_align(alignment);
  return SanitizerHelper::unpoison(
      do_allocate_already_aligned(bytes, alignment), bytes);
}

template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ExclusiveMonotonicBufferResource::register_destructor(T* ptr) noexcept {
  if (!::std::is_trivially_destructible<T>::value) {
    register_destructor(ptr, &destruct<T>);
  }
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ExclusiveMonotonicBufferResource::register_destructor(
    void* ptr, void (*destructor)(void*)) noexcept {
  auto task = get_destroy_task();
  task->destructor = destructor;
  task->ptr = ptr;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE
    ExclusiveMonotonicBufferResource::DestroyTask*
    ExclusiveMonotonicBufferResource::get_destroy_task() noexcept {
  if (_last_destroy_task_pointer != _last_destroy_task_array->tasks) {
    return SanitizerHelper::unpoison(--_last_destroy_task_pointer);
  }
  return SanitizerHelper::unpoison(do_get_destroy_task_in_new_array());
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE PageAllocator&
ExclusiveMonotonicBufferResource::page_allocator() noexcept {
  return *_page_allocator;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
ExclusiveMonotonicBufferResource::space_used() const noexcept {
  return _space_used;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
ExclusiveMonotonicBufferResource::space_allocated() const noexcept {
  return _space_allocated;
}

template <size_t alignment>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ExclusiveMonotonicBufferResource::do_align() noexcept {
  static_assert(::absl::has_single_bit(alignment), "alignment must be 2^n");
  do_align(alignment);
}

template <>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ExclusiveMonotonicBufferResource::do_align<1>() noexcept {}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ExclusiveMonotonicBufferResource::do_align(size_t alignment) noexcept {
  auto free_begin = reinterpret_cast<uintptr_t>(_free_begin);
  free_begin =
      (free_begin + alignment - 1) & static_cast<uintptr_t>(-alignment);
  _free_begin = reinterpret_cast<char*>(free_begin);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void*
ExclusiveMonotonicBufferResource::do_allocate_already_aligned(
    size_t bytes, size_t alignment) noexcept {
  _space_used += bytes;
  auto result = _free_begin;
  auto next = result + bytes;
  if (ABSL_PREDICT_TRUE(next <= _free_end)) {
    _free_begin = next;
    return result;
  }
  return do_allocate_in_new_page(bytes, alignment);
}
// ExclusiveMonotonicBufferResource end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SharedMonotonicBufferResource begin
template <size_t alignment>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void*
SharedMonotonicBufferResource::allocate(size_t bytes) noexcept {
  auto local = _resources.local_fast();
  if (ABSL_PREDICT_TRUE(local != nullptr)) {
    return local->allocate<alignment>(bytes);
  }
  return allocate_slow<alignment>(bytes);
}

template <size_t alignment>
ABSL_ATTRIBUTE_NOINLINE void* SharedMonotonicBufferResource::allocate_slow(
    size_t bytes) noexcept {
  return _resources.local().allocate<alignment>(bytes);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void*
SharedMonotonicBufferResource::allocate(size_t bytes,
                                        size_t alignment) noexcept {
  return _resources.local().allocate(bytes, alignment);
}

template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
SharedMonotonicBufferResource::register_destructor(T* ptr) noexcept {
  if (!::std::is_trivially_destructible<T>::value) {
    register_destructor(ptr, &destruct<T>);
  }
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
SharedMonotonicBufferResource::register_destructor(
    void* ptr, void (*destructor)(void*)) noexcept {
  _resources.local().register_destructor(ptr, destructor);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE SharedMonotonicBufferResource::DestroyTask*
SharedMonotonicBufferResource::get_destroy_task() noexcept {
  return _resources.local().get_destroy_task();
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE PageAllocator&
SharedMonotonicBufferResource::page_allocator() noexcept {
  return *_page_allocator;
}
// SharedMonotonicBufferResource end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

// clang-format off
#include "babylon/unprotect.h"
// clang-format on
