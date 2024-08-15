#include "babylon/reusable/page_allocator.h"

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// PageAllocator begin
PageAllocator::~PageAllocator() noexcept {}

void* PageAllocator::allocate() noexcept {
  void* page = nullptr;
  allocate(&page, 1);
  return page;
}

void PageAllocator::deallocate(void* page) noexcept {
  deallocate(&page, 1);
}
// PageAllocator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// NewDeletePageAllocator begin
void NewDeletePageAllocator::set_page_size(size_t page_size) noexcept {
  _page_size = ::absl::bit_ceil(page_size);
}

size_t NewDeletePageAllocator::page_size() const noexcept {
  return _page_size;
}

void NewDeletePageAllocator::allocate(void** pages, size_t num) noexcept {
  for (size_t i = 0; i < num; ++i) {
    pages[i] = ::operator new(_page_size, ::std::align_val_t(_page_size));
  }
}

void NewDeletePageAllocator::deallocate(void** pages, size_t num) noexcept {
  for (size_t i = 0; i < num; ++i) {
    ::operator delete(pages[i], _page_size, ::std::align_val_t(_page_size));
  }
}
// NewDeletePageAllocator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SystemPageAllocator begin
size_t SystemPageAllocator::page_size() const noexcept {
  return _allocator.page_size();
}

void SystemPageAllocator::allocate(void** pages, size_t num) noexcept {
  _allocator.allocate(pages, num);
}

void SystemPageAllocator::deallocate(void** pages, size_t num) noexcept {
  _allocator.deallocate(pages, num);
}

SystemPageAllocator& SystemPageAllocator::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static SystemPageAllocator object;
#pragma GCC diagnostic pop
  return object;
}

SystemPageAllocator::SystemPageAllocator() noexcept {
  _allocator.set_page_size(static_cast<size_t>(::sysconf(_SC_PAGE_SIZE)));
}
// SystemPageAllocator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CachedPageAllocator begin
CachedPageAllocator::~CachedPageAllocator() noexcept {
  _free_pages.try_pop_n<false, false>(
      [&](Iterator iter, Iterator end) {
        while (iter != end) {
          _upstream->deallocate(*iter++);
        }
      },
      _free_pages.capacity());
}

void CachedPageAllocator::set_upstream(PageAllocator& upstream) noexcept {
  _upstream = &upstream;
}

void CachedPageAllocator::set_free_page_capacity(size_t capacity) noexcept {
  _free_pages.reserve_and_clear(capacity);
}

size_t CachedPageAllocator::page_size() const noexcept {
  return _upstream->page_size();
}

void CachedPageAllocator::allocate(void** pages, size_t num) noexcept {
  auto pages_end = pages + num;
  auto need_pop_num = ::std::min(num, _free_pages.capacity());
  ssize_t hit_num = static_cast<ssize_t>(need_pop_num);
  // 优先从缓存队列中申请，穿透时从上游申请作为补偿
  _free_pages.pop_n(
      [&](Iterator begin, Iterator end) {
        pages = ::std::copy(begin, end, pages);
      },
      [&](Iterator iter, Iterator end) {
        hit_num = ::std::max<ssize_t>(hit_num - (end - iter), 0);
        while (iter != end) {
          *iter++ = _upstream->allocate();
        }
      },
      need_pop_num);
  // 超过缓存容量部分同样直接从上游申请
  while (pages < pages_end) {
    *pages++ = _upstream->allocate();
  }
  _cache_hit << ConcurrentSummer::Summary {hit_num, num};
}

void CachedPageAllocator::deallocate(void** pages, size_t num) noexcept {
  auto pages_end = pages + num;
  auto need_push_num = ::std::min<size_t>(num, _free_pages.capacity());
  // 优先释放到缓存队列中，溢出时释放给上游作为补偿
  _free_pages.push_n(
      [&](Iterator begin, Iterator end) {
        auto n = static_cast<size_t>(end - begin);
        ::std::copy_n(pages, n, begin);
        pages += n;
      },
      [&](Iterator iter, Iterator end) {
        while (iter != end) {
          _upstream->deallocate(*iter++);
        }
      },
      need_push_num);
  // 超过缓存容量部分同样直接释放给上游
  while (pages < pages_end) {
    _upstream->deallocate(*pages++);
  }
}

size_t CachedPageAllocator::free_page_num() const noexcept {
  return _free_pages.size();
}

size_t CachedPageAllocator::free_page_capacity() const noexcept {
  return _free_pages.capacity();
}

ConcurrentSummer::Summary CachedPageAllocator::cache_hit_summary()
    const noexcept {
  return _cache_hit.value();
}
// CachedPageAllocator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BatchPageAllocator begin
BatchPageAllocator::~BatchPageAllocator() noexcept {
  _cache.for_each([&](Slot* iter, Slot* end) {
    while (iter != end) {
      auto& local = *iter++;
      if (local.next_page < local.buffer.end()) {
        _upstream->deallocate(
            &*local.next_page,
            static_cast<size_t>(local.buffer.end() - local.next_page));
      }
    }
  });
}

void BatchPageAllocator::set_upstream(PageAllocator& upstream) noexcept {
  _upstream = &upstream;
}

void BatchPageAllocator::set_batch_size(size_t batch_size) noexcept {
  _batch_size = batch_size;
  _cache.set_constructor([&](Slot* local) {
    new (local) Slot;
    local->buffer.resize(_batch_size);
    local->next_page = local->buffer.end();
  });
}

size_t BatchPageAllocator::page_size() const noexcept {
  return _upstream->page_size();
}

void* BatchPageAllocator::allocate() noexcept {
  auto& local = _cache.local();
  if (local.next_page < local.buffer.end()) {
    return *local.next_page++;
  }
  _upstream->allocate(local.buffer.data(), _batch_size);
  local.next_page = local.buffer.begin() + 1;
  return *local.buffer.data();
}

void BatchPageAllocator::allocate(void** pages, size_t num) noexcept {
  for (size_t i = 0; i < num; ++i) {
    pages[i] = allocate();
  }
}

void BatchPageAllocator::deallocate(void** pages, size_t num) noexcept {
  _upstream->deallocate(pages, num);
}
// BatchPageAllocator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CountingPageAllocator begin
void CountingPageAllocator::set_upstream(PageAllocator& upstream) noexcept {
  _upstream = &upstream;
}

size_t CountingPageAllocator::allocated_page_num() noexcept {
  return static_cast<size_t>(
      ::std::max<ssize_t>(0, _allocate_page_num.value()));
}

size_t CountingPageAllocator::page_size() const noexcept {
  return _upstream->page_size();
}

void* CountingPageAllocator::allocate() noexcept {
  _allocate_page_num << 1;
  return _upstream->allocate();
}

void CountingPageAllocator::allocate(void** pages, size_t num) noexcept {
  _allocate_page_num << num;
  _upstream->allocate(pages, num);
}

void CountingPageAllocator::deallocate(void* page) noexcept {
  _allocate_page_num << -1;
  _upstream->deallocate(page);
}

void CountingPageAllocator::deallocate(void** pages, size_t num) noexcept {
  _allocate_page_num << -num;
  _upstream->deallocate(pages, num);
}
// CountingPageAllocator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PageHeap begin
PageHeap::PageHeap() noexcept {
  _cached_allocator.set_free_page_capacity(1024);
}

void PageHeap::set_page_size(size_t page_size) noexcept {
  if (page_size == SystemPageAllocator::instance().page_size()) {
    _cached_allocator.set_upstream(SystemPageAllocator::instance());
  } else {
    _base_allocator.set_page_size(page_size);
    _cached_allocator.set_upstream(_base_allocator);
  }
}

void PageHeap::set_free_page_capacity(size_t capacity) noexcept {
  _cached_allocator.set_free_page_capacity(capacity);
}

size_t PageHeap::page_size() const noexcept {
  return _cached_allocator.page_size();
}

void PageHeap::allocate(void** pages, size_t num) noexcept {
  _cached_allocator.allocate(pages, num);
  _allocate_page_num << num;
}

void PageHeap::deallocate(void** pages, size_t num) noexcept {
  _cached_allocator.deallocate(pages, num);
  _allocate_page_num << -num;
}

size_t PageHeap::allocate_page_num() const noexcept {
  return static_cast<size_t>(
      ::std::max<ssize_t>(0, _allocate_page_num.value()));
}

size_t PageHeap::free_page_num() const noexcept {
  return _cached_allocator.free_page_num();
}

size_t PageHeap::free_page_capacity() const noexcept {
  return _cached_allocator.free_page_capacity();
}

typename ConcurrentSummer::Summary PageHeap::cache_hit_summary()
    const noexcept {
  return _cached_allocator.cache_hit_summary();
}

PageHeap& PageHeap::system_page_heap() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static PageHeap object;
#pragma GCC diagnostic pop
  return object;
}

PageHeap::PageHeap(size_t free_page_capacity) noexcept {
  set_free_page_capacity(free_page_capacity);
}

PageHeap::PageHeap(size_t free_page_capacity, size_t page_size) noexcept {
  set_free_page_capacity(free_page_capacity);
  set_page_size(page_size);
}
// PageHeap end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
