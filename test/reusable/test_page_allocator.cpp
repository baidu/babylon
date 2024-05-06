#include "babylon/reusable/page_allocator.h"

#include "gtest/gtest.h"

#include <thread>

using ::babylon::BatchPageAllocator;
using ::babylon::CachedPageAllocator;
using ::babylon::NewDeletePageAllocator;
using ::babylon::PageAllocator;
using ::babylon::PageHeap;
using ::babylon::SystemPageAllocator;

struct MockPageAllocator : public NewDeletePageAllocator {
  virtual void allocate(void** pages, size_t num) noexcept override {
    allocate_times.fetch_add(1);
    allocate_pages.fetch_add(num);
    NewDeletePageAllocator::allocate(pages, num);
  }
  virtual void deallocate(void** pages, size_t num) noexcept override {
    deallocate_times.fetch_add(1);
    deallocate_pages.fetch_add(num);
    NewDeletePageAllocator::deallocate(pages, num);
  }

  ::std::atomic<size_t> allocate_times {0};
  ::std::atomic<size_t> allocate_pages {0};
  ::std::atomic<size_t> deallocate_times {0};
  ::std::atomic<size_t> deallocate_pages {0};
};

static size_t kernel_page_size = ::sysconf(_SC_PAGESIZE);

TEST(system_page_allocator, allocate_valid_page) {
  SystemPageAllocator& allocator = SystemPageAllocator::instance();
  ASSERT_EQ(kernel_page_size, allocator.page_size());
  void* page = allocator.allocate();
  ASSERT_EQ(0, reinterpret_cast<uintptr_t>(page) % kernel_page_size);
  ::memset(page, 0, kernel_page_size);
  allocator.deallocate(page);
}

TEST(cached_page_allocator, proxy_page_size_to_upstream) {
  NewDeletePageAllocator upstream_allocator;
  CachedPageAllocator allocator;
  upstream_allocator.set_page_size(128);
  allocator.set_upstream(upstream_allocator);
  ASSERT_EQ(128, allocator.page_size());
}

TEST(cached_page_allocator, proxy_allocate_to_upstream_when_empty) {
  NewDeletePageAllocator upstream_allocator;
  CachedPageAllocator allocator;
  allocator.set_upstream(upstream_allocator);
  void* pages[2];
  pages[0] = allocator.allocate();
  pages[1] = allocator.allocate();
  ASSERT_NE(pages[0], pages[1]);
  ASSERT_EQ(0, allocator.cache_hit_summary().sum);
  ASSERT_EQ(2, allocator.cache_hit_summary().num);
  allocator.deallocate(pages, 2);
}

TEST(cached_page_allocator, allocate_free_pages_when_available) {
  NewDeletePageAllocator upstream_allocator;
  CachedPageAllocator allocator;
  allocator.set_upstream(upstream_allocator);
  void* pages[2];
  pages[0] = allocator.allocate();
  ASSERT_EQ(0, allocator.free_page_num());
  allocator.deallocate(pages[0]);
  ASSERT_EQ(1, allocator.free_page_num());
  pages[1] = allocator.allocate();
  ASSERT_EQ(0, allocator.free_page_num());
  ASSERT_EQ(pages[0], pages[1]);
  ASSERT_EQ(1, allocator.cache_hit_summary().sum);
  ASSERT_EQ(2, allocator.cache_hit_summary().num);
  allocator.deallocate(pages, 1);
}

TEST(cached_page_allocator, support_large_batch) {
  NewDeletePageAllocator upstream_allocator;
  CachedPageAllocator allocator;
  allocator.set_upstream(upstream_allocator);
  allocator.set_free_page_capacity(2);
  void* pages[10];
  allocator.allocate(pages, 10);
  for (auto page : pages) {
    ::memset(page, 0, allocator.page_size());
  }
  allocator.deallocate(pages, 10);
}

TEST(batch_page_allocator, page_size_same_to_upstream) {
  MockPageAllocator upstream_allocator;
  BatchPageAllocator allocator;
  allocator.set_upstream(upstream_allocator);
  upstream_allocator.set_page_size(1024);
  ASSERT_EQ(upstream_allocator.page_size(), allocator.page_size());
  upstream_allocator.set_page_size(4096);
  ASSERT_EQ(upstream_allocator.page_size(), allocator.page_size());
}

TEST(batch_page_allocator, allocate_aggragate_to_batch) {
  MockPageAllocator upstream_allocator;
  BatchPageAllocator allocator;
  allocator.set_upstream(upstream_allocator);
  upstream_allocator.set_page_size(1024);
  allocator.set_batch_size(32);
  ::std::vector<void*> pages;
  pages.emplace_back(allocator.allocate());
  ASSERT_EQ(1, upstream_allocator.allocate_times);
  ASSERT_EQ(32, upstream_allocator.allocate_pages);
  pages.resize(32);
  allocator.allocate(pages.data() + 1, 15);
  for (size_t i = 0; i < 16; ++i) {
    pages.emplace_back(allocator.allocate());
  }
  ASSERT_EQ(1, upstream_allocator.allocate_times);
  ASSERT_EQ(32, upstream_allocator.allocate_pages);
  pages.emplace_back(allocator.allocate());
  ASSERT_EQ(2, upstream_allocator.allocate_times);
  ASSERT_EQ(64, upstream_allocator.allocate_pages);
  for (auto page : pages) {
    allocator.deallocate(page);
  }
}

TEST(page_heap, page_size_auto_ceiled) {
  ASSERT_EQ(1, (PageHeap {1024, 0}).page_size());
  ASSERT_EQ(1, (PageHeap {1024, 1}).page_size());
  ASSERT_EQ(2, (PageHeap {1024, 2}).page_size());
  ASSERT_EQ(4, (PageHeap {1024, 3}).page_size());
  ASSERT_EQ(4, (PageHeap {1024, 4}).page_size());
  ASSERT_EQ(8, (PageHeap {1024, 5}).page_size());
  ASSERT_EQ(8, (PageHeap {1024, 6}).page_size());
  ASSERT_EQ(8, (PageHeap {1024, 7}).page_size());
  ASSERT_EQ(8, (PageHeap {1024, 8}).page_size());
  ASSERT_EQ(8, (PageHeap {1024, 8}).page_size());
}

TEST(page_heap, page_size_is_adjustable) {
  PageHeap page_heap;
  page_heap.set_page_size(1024);
  ASSERT_EQ(1024, page_heap.page_size());
  page_heap.set_page_size(kernel_page_size);
  ASSERT_EQ(kernel_page_size, page_heap.page_size());
  page_heap.set_page_size(8192);
  ASSERT_EQ(8192, page_heap.page_size());
}

TEST(page_heap, count_allocated_and_free_num) {
  PageHeap page_heap;
  ASSERT_EQ(0, page_heap.allocate_page_num());
  auto page = page_heap.allocate();
  ASSERT_EQ(1, page_heap.allocate_page_num());
  page_heap.deallocate(page);
  ASSERT_EQ(0, page_heap.allocate_page_num());
  ASSERT_EQ(1, page_heap.free_page_num());
}

TEST(page_heap, free_page_capacity_auto_ceiled) {
  ASSERT_EQ(1, (PageHeap {0}).free_page_capacity());
  ASSERT_EQ(1, (PageHeap {1}).free_page_capacity());
  ASSERT_EQ(2, (PageHeap {2}).free_page_capacity());
  ASSERT_EQ(4, (PageHeap {3}).free_page_capacity());
  ASSERT_EQ(4, (PageHeap {4}).free_page_capacity());
  ASSERT_EQ(8, (PageHeap {5}).free_page_capacity());
  ASSERT_EQ(8, (PageHeap {6}).free_page_capacity());
  ASSERT_EQ(8, (PageHeap {7}).free_page_capacity());
  ASSERT_EQ(8, (PageHeap {8}).free_page_capacity());
}

TEST(page_heap, acquire_new_allocated_page_when_no_free_left) {
  PageHeap page_heap;
  ::std::vector<void*> pages(2);
  pages[0] = page_heap.allocate();
  pages[1] = page_heap.allocate();
  ASSERT_NE(pages[0], pages[1]);
  page_heap.deallocate(&pages[0], 2);
}

TEST(page_heap, acquire_free_page_when_available) {
  PageHeap page_heap;
  ::std::vector<void*> pages(2);
  pages[0] = page_heap.allocate();
  page_heap.deallocate(&pages[0], 1);
  pages[1] = page_heap.allocate();
  ASSERT_EQ(pages[0], pages[1]);
  page_heap.deallocate(&pages[1], 1);
}

TEST(page_heap, allocate_page_in_batch) {
  PageHeap page_heap;
  ::std::vector<void*> pages(7);
  page_heap.allocate(&pages[0], 3);
  ASSERT_NE(pages[0], pages[1]);
  ASSERT_NE(pages[0], pages[2]);
  ASSERT_NE(pages[1], pages[2]);
  page_heap.deallocate(&pages[0], 3);
  page_heap.allocate(&pages[3], 2);
  ASSERT_EQ(pages[0], pages[3]);
  ASSERT_EQ(pages[1], pages[4]);
  page_heap.allocate(&pages[5], 2);
  ASSERT_EQ(pages[2], pages[5]);
  ASSERT_NE(nullptr, pages[6]);
  *static_cast<char*>(pages[6]) = '\0';
  page_heap.deallocate(&pages[3], 4);
}

TEST(page_heap, system_page_heap_with_cache) {
  PageHeap& page_heap = PageHeap::system_page_heap();
  ASSERT_LT(0, page_heap.free_page_capacity());
  ASSERT_EQ(kernel_page_size, page_heap.page_size());
}
