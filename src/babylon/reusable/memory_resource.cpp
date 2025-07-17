#include "babylon/reusable/memory_resource.h"

#include "babylon/new.h"

#if __cpp_lib_memory_resource < 201603L
namespace std {
namespace pmr {

memory_resource::~memory_resource() {};

memory_resource* new_delete_resource() noexcept {
  struct R final : public memory_resource {
    virtual void* do_allocate(size_t bytes,
                              size_t alignment) noexcept override {
      return ::operator new(bytes, ::std::align_val_t(alignment));
    }

    virtual void do_deallocate(void* ptr, size_t bytes,
                               size_t alignment) noexcept override {
      ::operator delete(ptr, bytes, ::std::align_val_t(alignment));
    }

    virtual bool do_is_equal(
        const memory_resource& __other) const noexcept override {
      return this == &__other;
    }
  };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static R resource;
#pragma GCC diagnostic pop
  return &resource;
}

} // namespace pmr
} // namespace std
#endif // __cpp_lib_memory_resource < 201603L

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// MonotonicBufferResource begin
bool MonotonicBufferResource::contains(const void*) noexcept {
  return false;
}

size_t MonotonicBufferResource::space_used() const noexcept {
  return 0;
}

size_t MonotonicBufferResource::space_allocated() const noexcept {
  return 0;
}

void MonotonicBufferResource::do_deallocate(void*, size_t, size_t) noexcept {}

bool MonotonicBufferResource::do_is_equal(
    const ::std::pmr::memory_resource& other) const noexcept {
  return &other == this;
}
// MonotonicBufferResource end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ExclusiveMonotonicBufferResource begin
size_t ExclusiveMonotonicBufferResource::allocate_oversize_page_num() noexcept {
  return static_cast<size_t>(oversize_page_concurrent_adder().value());
}

ExclusiveMonotonicBufferResource::ExclusiveMonotonicBufferResource(
    ExclusiveMonotonicBufferResource&& other) noexcept
    : ExclusiveMonotonicBufferResource {} {
  *this = ::std::move(other);
}

ExclusiveMonotonicBufferResource& ExclusiveMonotonicBufferResource::operator=(
    ExclusiveMonotonicBufferResource&& other) noexcept {
  ::std::swap(_page_allocator, other._page_allocator);

  ::std::swap(_last_page_array, other._last_page_array);
  ::std::swap(_last_page_pointer, other._last_page_pointer);
  ::std::swap(_free_begin, other._free_begin);
  ::std::swap(_free_end, other._free_end);
  ::std::swap(_space_used, other._space_used);
  ::std::swap(_space_allocated, other._space_allocated);

  ::std::swap(_last_oversize_page_array, other._last_oversize_page_array);
  ::std::swap(_last_oversize_page_pointer, other._last_oversize_page_pointer);

  ::std::swap(_last_destroy_task_array, other._last_destroy_task_array);
  ::std::swap(_last_destroy_task_pointer, other._last_destroy_task_pointer);
  return *this;
}

ExclusiveMonotonicBufferResource::~ExclusiveMonotonicBufferResource() noexcept {
  release();
}

void ExclusiveMonotonicBufferResource::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  assert((_page_allocator == &page_allocator || _space_allocated == 0) &&
         "can not change page_allocator after allocate");
  assert((page_allocator.page_size() >= sizeof(PageArray)) &&
         "can not set page_allocator with too small page size");
  _page_allocator = &page_allocator;
}

void ExclusiveMonotonicBufferResource::set_upstream(
    ::std::pmr::memory_resource& upstream) noexcept {
  assert((_upstream == &upstream || _space_allocated == 0) &&
         "can not change upstream after allocate");
  _upstream = &upstream;
}

void ExclusiveMonotonicBufferResource::destruct_all() noexcept {
  while (_last_destroy_task_array != nullptr) {
    SanitizerHelper::PoisonGuard guard {_last_destroy_task_array};
    auto array = _last_destroy_task_array;
    auto end = &array->tasks[DESTROY_TASK_ARRAY_CAPACITY];
    for (auto task = _last_destroy_task_pointer; task != end; ++task) {
      task->destructor(task->ptr);
    }
    _last_destroy_task_array = array->next;
    _last_destroy_task_pointer = _last_destroy_task_array->tasks;
  }
}

void* ExclusiveMonotonicBufferResource::do_allocate(size_t bytes,
                                                    size_t alignment) noexcept {
  return allocate(bytes, alignment);
}

void* ExclusiveMonotonicBufferResource::do_allocate_in_new_page(
    size_t bytes, size_t alignment) noexcept {
  auto page_size = _page_allocator->page_size();
  if (bytes <= page_size && alignment <= page_size) {
    auto page = reinterpret_cast<char*>(_page_allocator->allocate());
    SanitizerHelper::PoisonGuard page_guard {page, page_size};
    _space_allocated += page_size;
    if (_last_page_pointer > _last_page_array->pages) {
      SanitizerHelper::PoisonGuard guard {_last_page_array};
      *--_last_page_pointer = page;
      _free_begin = page + bytes;
      _free_end = page + page_size;
      return page;
    } else {
      return do_allocate_with_page_in_new_page_array(bytes, page);
    }
  }
  return do_allocate_in_oversize_page(bytes, alignment);
}

void* ExclusiveMonotonicBufferResource::do_allocate_with_page_in_new_page_array(
    size_t bytes, char* page) noexcept {
  auto page_size = _page_allocator->page_size();
  do_align<alignof(PageArray)>();
  if (_free_begin + sizeof(PageArray) <= _free_end) {
    SanitizerHelper::PoisonGuard last_guard {_last_page_array};
    auto new_page_array = reinterpret_cast<PageArray*>(_free_begin);
    SanitizerHelper::PoisonGuard new_guard {new_page_array};
    new_page_array->next = _last_page_array;
    _last_page_array = new_page_array;
    _last_page_pointer = &new_page_array->pages[PAGE_ARRAY_CAPACITY - 1];
    *_last_page_pointer = page;
    _free_begin = page + bytes;
    _free_end = page + page_size;
  } else if (bytes + sizeof(PageArray) <= page_size) {
    bytes = (bytes + alignof(PageArray) - 1) &
            static_cast<size_t>(-alignof(PageArray));
    auto new_page_array = reinterpret_cast<PageArray*>(page + bytes);
    new_page_array->next = _last_page_array;
    _last_page_array = new_page_array;
    _last_page_pointer = &new_page_array->pages[PAGE_ARRAY_CAPACITY - 1];
    *_last_page_pointer = page;
    _free_begin = page + bytes + sizeof(PageArray);
    _free_end = page + page_size;
  } else {
    auto additional_page = reinterpret_cast<char*>(_page_allocator->allocate());
    _space_allocated += page_size;
    auto new_page_array = reinterpret_cast<PageArray*>(additional_page);
    new_page_array->next = _last_page_array;
    _last_page_array = new_page_array;
    new_page_array->pages[PAGE_ARRAY_CAPACITY - 1] = page;
    _last_page_pointer = &new_page_array->pages[PAGE_ARRAY_CAPACITY - 2];
    *_last_page_pointer = additional_page;
    _free_begin = additional_page + sizeof(PageArray);
    _free_end = additional_page + page_size;
    SanitizerHelper::poison(additional_page, page_size);
  }
  return page;
}

void* ExclusiveMonotonicBufferResource::do_allocate_in_oversize_page(
    size_t bytes, size_t alignment) noexcept {
  oversize_page_concurrent_adder() << 1;
  if (_last_oversize_page_pointer > _last_oversize_page_array->pages) {
    auto page = reinterpret_cast<char*>(_upstream->allocate(bytes, alignment));
    SanitizerHelper::PoisonGuard guard {_last_oversize_page_array};
    _space_allocated += bytes;
    --_last_oversize_page_pointer;
    _last_oversize_page_pointer->page = page;
    _last_oversize_page_pointer->bytes = bytes;
    _last_oversize_page_pointer->alignment = alignment;
    SanitizerHelper::poison(page, bytes);
    return page;
  }
  alignment = ::std::max(alignment, alignof(OversizePageArray));
  bytes = (bytes + alignment - 1) & static_cast<size_t>(-alignment);
  auto page = reinterpret_cast<char*>(
      _upstream->allocate(bytes + sizeof(OversizePageArray), alignment));
  _space_allocated += bytes + sizeof(OversizePageArray);
  auto new_page_array = reinterpret_cast<OversizePageArray*>(page + bytes);
  new_page_array->next = _last_oversize_page_array;
  _last_oversize_page_array = new_page_array;
  _last_oversize_page_pointer =
      &new_page_array->pages[DESTROY_TASK_ARRAY_CAPACITY - 1];
  _last_oversize_page_pointer->page = page;
  _last_oversize_page_pointer->bytes = bytes + sizeof(OversizePageArray);
  _last_oversize_page_pointer->alignment = alignment;
  SanitizerHelper::poison(_last_oversize_page_pointer->page,
                          _last_oversize_page_pointer->bytes);
  return page;
}

void ExclusiveMonotonicBufferResource::do_register_destructor(
    void* ptr, void (*destructor)(void*)) noexcept {
  return register_destructor(ptr, destructor);
}

ExclusiveMonotonicBufferResource::DestroyTask*
ExclusiveMonotonicBufferResource::do_get_destroy_task_in_new_array() noexcept {
  auto array = reinterpret_cast<DestroyTaskArray*>(
      allocate<alignof(DestroyTaskArray)>(sizeof(DestroyTaskArray)));
  SanitizerHelper::PoisonGuard guard {array};
  array->next = _last_destroy_task_array;
  _last_destroy_task_array = array;
  _last_destroy_task_pointer = &array->tasks[DESTROY_TASK_ARRAY_CAPACITY - 1];
  return _last_destroy_task_pointer;
}

void ExclusiveMonotonicBufferResource::release() noexcept {
  destruct_all();
  char* tmp_pages[PAGE_ARRAY_CAPACITY];
  while (_last_page_array != nullptr) {
    SanitizerHelper::unpoison(_last_page_array);
    auto size = static_cast<size_t>(_last_page_array->pages +
                                    PAGE_ARRAY_CAPACITY - _last_page_pointer);
    _last_page_array = _last_page_array->next;

    // 将pages的地址提前拷一份存起来，
    // 避免当page_rray持有自己时，page_rray所在的page被提前释放后_last_page_pointer失效
    for (size_t i = 0; i < size; ++i) {
      tmp_pages[i] = SanitizerHelper::unpoison(_last_page_pointer[i],
                                               _page_allocator->page_size());
    }
    _page_allocator->deallocate(reinterpret_cast<void**>(tmp_pages), size);
    _last_page_pointer = _last_page_array->pages;
    _free_begin = nullptr;
    _free_end = nullptr;
  }
  while (_last_oversize_page_array != nullptr) {
    SanitizerHelper::unpoison(_last_oversize_page_array);
    auto iter = _last_oversize_page_pointer;
    auto end = _last_oversize_page_array->pages + PAGE_ARRAY_CAPACITY;
    _last_oversize_page_array = _last_oversize_page_array->next;
    while (iter != end) {
      SanitizerHelper::unpoison(iter->page, iter->bytes);
      _upstream->deallocate(iter->page, iter->bytes, iter->alignment);
      ++iter;
    }
    _last_oversize_page_pointer = _last_oversize_page_array->pages;
  }
  _space_used = 0;
  _space_allocated = 0;
}

bool ExclusiveMonotonicBufferResource::contains(const void* ptr) noexcept {
  {
    auto page_size = _page_allocator->page_size();
    auto page_array = _last_page_array;
    auto iter = _last_page_pointer;
    auto size = page_size - static_cast<size_t>(_free_end - _free_begin);
    while (page_array != nullptr) {
      SanitizerHelper::PoisonGuard guard {page_array};
      auto end = &page_array->pages[PAGE_ARRAY_CAPACITY];
      for (; iter < end; ++iter) {
        if (ptr >= *iter && ptr < *iter + size) {
          return true;
        }
        size = page_size;
      }
      page_array = page_array->next;
      iter = page_array->pages;
    }
  }
  {
    auto page_array = _last_oversize_page_array;
    auto iter = _last_oversize_page_pointer;
    while (page_array != nullptr) {
      SanitizerHelper::PoisonGuard guard {page_array};
      auto end = &page_array->pages[PAGE_ARRAY_CAPACITY];
      for (; iter < end; ++iter) {
        if (ptr >= iter->page && ptr < iter->page + iter->bytes) {
          return true;
        }
      }
      page_array = page_array->next;
      iter = page_array->pages;
    }
  }
  return false;
}

ConcurrentAdder&
ExclusiveMonotonicBufferResource::oversize_page_concurrent_adder() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static ConcurrentAdder _oversize_page_concurrent_adder;
#pragma GCC diagnostic pop
  return _oversize_page_concurrent_adder;
}
// ExclusiveMonotonicBufferResource end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SharedMonotonicBufferResource begin
SharedMonotonicBufferResource::~SharedMonotonicBufferResource() noexcept {
  release();
}

SharedMonotonicBufferResource::SharedMonotonicBufferResource(
    PageAllocator& page_allocator) noexcept {
  set_page_allocator(page_allocator);
}

void SharedMonotonicBufferResource::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  if (_page_allocator != &page_allocator) {
    _page_allocator = &page_allocator;
    register_thread_constructor();
  }
}

void SharedMonotonicBufferResource::set_upstream(
    ::std::pmr::memory_resource& upstream) noexcept {
  if (_upstream != &upstream) {
    _upstream = &upstream;
    register_thread_constructor();
  }
}

void* SharedMonotonicBufferResource::do_allocate(size_t bytes,
                                                 size_t alignment) noexcept {
  return _resources.local().allocate(bytes, alignment);
}

void SharedMonotonicBufferResource::do_register_destructor(
    void* ptr, void (*destructor)(void*)) noexcept {
  return _resources.local().register_destructor(ptr, destructor);
}

void SharedMonotonicBufferResource::release() noexcept {
  _resources.for_each([](ExclusiveMonotonicBufferResource* iter,
                         ExclusiveMonotonicBufferResource* end) {
    while (iter != end) {
      iter++->destruct_all();
    }
  });
  _resources.for_each([](ExclusiveMonotonicBufferResource* iter,
                         ExclusiveMonotonicBufferResource* end) {
    while (iter != end) {
      iter++->release();
    }
  });
}

bool SharedMonotonicBufferResource::contains(const void* ptr) noexcept {
  bool result = false;
  _resources.for_each([&](ExclusiveMonotonicBufferResource* iter,
                          ExclusiveMonotonicBufferResource* end) {
    while (iter != end) {
      if (static_cast<MonotonicBufferResource*>(iter++)->contains(ptr)) {
        result = true;
        break;
      }
    }
  });
  return result;
}

size_t SharedMonotonicBufferResource::space_used() const noexcept {
  size_t result = 0;
  _resources.for_each([&](const ExclusiveMonotonicBufferResource* iter,
                          const ExclusiveMonotonicBufferResource* end) {
    while (iter != end) {
      result += (iter++)->space_used();
    }
  });
  return result;
}

size_t SharedMonotonicBufferResource::space_allocated() const noexcept {
  size_t result = 0;
  _resources.for_each([&](const ExclusiveMonotonicBufferResource* iter,
                          const ExclusiveMonotonicBufferResource* end) {
    while (iter != end) {
      result += (iter++)->space_allocated();
    }
  });
  return result;
}

void SharedMonotonicBufferResource::register_thread_constructor() noexcept {
  assert(space_allocated() == 0 &&
         "can not change page_allocator or upstream after allocate");
  _resources.set_constructor([&](ExclusiveMonotonicBufferResource* ptr) {
    new (ptr) ExclusiveMonotonicBufferResource;
    ptr->set_page_allocator(*_page_allocator);
    ptr->set_upstream(*_upstream);
  });
}
// SharedMonotonicBufferResource end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SwissMemoryResource begin
void SwissMemoryResource::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
#if GOOGLE_PROTOBUF_VERSION >= 3000000
  _arena.store(nullptr, ::std::memory_order_relaxed);
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000
  SharedMonotonicBufferResource::release();
  SharedMonotonicBufferResource::set_page_allocator(page_allocator);
}
void SwissMemoryResource::set_upstream(
    ::std::pmr::memory_resource& upstream) noexcept {
#if GOOGLE_PROTOBUF_VERSION >= 3000000
  _arena.store(nullptr, ::std::memory_order_relaxed);
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000
  SharedMonotonicBufferResource::release();
  SharedMonotonicBufferResource::set_upstream(upstream);
}
void* SwissMemoryResource::arena_block_alloc(size_t size) noexcept {
  return ::operator new(size);
}
void SwissMemoryResource::arena_block_dealloc(void* ptr, size_t size) noexcept {
  ::operator delete(ptr, size);
}
void SwissMemoryResource::release() noexcept {
#if GOOGLE_PROTOBUF_VERSION >= 3000000
  _arena.store(nullptr, ::std::memory_order_relaxed);
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000
  SharedMonotonicBufferResource::release();
}
// SwissMemoryResource end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
