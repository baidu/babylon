#pragma once

#include "babylon/concurrent/bounded_queue.h" // ConcurrentBoundedQueue
#include "babylon/concurrent/counter.h"       // ConcurrentAdder
#include "babylon/environment.h"
#include "babylon/protect.h"

#include <vector>

BABYLON_NAMESPACE_BEGIN

// 整页内存分配器接口
// 和传统内存分配器，例如std::allocator的主要区别是
// 只支持唯一固定长度的内存块分配释放
// 由于不用考虑碎片问题，一般可以有更高效的实现方法
class PageAllocator {
 public:
  virtual ~PageAllocator() noexcept = 0;

  // 分配释放的内存块大小
  virtual size_t page_size() const noexcept = 0;

  // 分配释放接口
  virtual void* allocate() noexcept;
  virtual void allocate(void** pages, size_t num) noexcept = 0;
  virtual void deallocate(void* page) noexcept;
  virtual void deallocate(void** pages, size_t num) noexcept = 0;
};

// 直接采用new/delete实现的基础分配器
class NewDeletePageAllocator : public PageAllocator {
 public:
  // 设置分配释放内存块大小
  // 需要在使用前完成调整
  void set_page_size(size_t page_size) noexcept;

  // PageAllocator接口实现
  virtual size_t page_size() const noexcept override;
  using PageAllocator::allocate;
  virtual void allocate(void** pages, size_t num) noexcept override;
  using PageAllocator::deallocate;
  virtual void deallocate(void** pages, size_t num) noexcept override;

 private:
  size_t _page_size {8};
};

// 相当于一个特殊尺寸的NewDeletePageAllocator
// 固定为系统的PAGE_SIZE
class SystemPageAllocator : public PageAllocator {
 public:
  // PageAllocator接口实现
  virtual size_t page_size() const noexcept override;
  using PageAllocator::allocate;
  virtual void allocate(void** pages, size_t num) noexcept override;
  using PageAllocator::deallocate;
  virtual void deallocate(void** pages, size_t num) noexcept override;

  // 获取无状态全局单例
  static SystemPageAllocator& instance() noexcept;

 private:
  SystemPageAllocator() noexcept;
  SystemPageAllocator(SystemPageAllocator&&) = delete;
  SystemPageAllocator(const SystemPageAllocator&) = delete;
  SystemPageAllocator& operator=(SystemPageAllocator&&) = delete;
  SystemPageAllocator& operator=(const SystemPageAllocator&) = delete;

  NewDeletePageAllocator _allocator;
};

// 带缓存的分配器，实际分配动作最终由【上游】分配器执行
// 释放时会放入缓存队列中供后续分配时优先重复利用
class CachedPageAllocator : public PageAllocator {
 public:
  // 析构时释放缓存中的内存块
  virtual ~CachedPageAllocator() noexcept override;

  // 设置上游分配器，缓存不足时通过上游分配
  void set_upstream(PageAllocator& upstream) noexcept;
  // 设置缓存容量，释放溢出时会通过上游释放
  void set_free_page_capacity(size_t capacity) noexcept;

  // PageAllocator接口实现
  virtual size_t page_size() const noexcept override;
  using PageAllocator::allocate;
  virtual void allocate(void** pages, size_t num) noexcept override;
  using PageAllocator::deallocate;
  virtual void deallocate(void** pages, size_t num) noexcept override;

  // 当前缓存量
  size_t free_page_num() const noexcept;
  // 缓存容量
  size_t free_page_capacity() const noexcept;
  // 命中统计量
  // sum为命中次数
  // num为总次数
  ConcurrentSummer::Summary cache_hit_summary() const noexcept;

 private:
  using Queue = ConcurrentBoundedQueue<void*>;
  using Iterator = Queue::Iterator;

  Queue _free_pages;
  PageAllocator* _upstream {&SystemPageAllocator::instance()};
  ConcurrentSummer _cache_hit;
};

// 一次性从上游批量申请页并存入到线程缓存
// 用于降低大并发下的竞争开销
class BatchPageAllocator : public PageAllocator {
 public:
  BatchPageAllocator() noexcept = default;
  BatchPageAllocator(BatchPageAllocator&&) noexcept = default;
  BatchPageAllocator(const BatchPageAllocator&) noexcept = delete;
  BatchPageAllocator& operator=(BatchPageAllocator&&) noexcept = default;
  BatchPageAllocator& operator=(const BatchPageAllocator&) noexcept = delete;
  // 析构时释放预取到线程缓存中的页
  virtual ~BatchPageAllocator() noexcept override;

  // 设置上游分配器，缓存不足时通过上游分配
  void set_upstream(PageAllocator& upstream) noexcept;
  // 设置批量分配大小
  void set_batch_size(size_t batch_size) noexcept;

  // PageAllocator接口实现
  virtual size_t page_size() const noexcept override;
  virtual void* allocate() noexcept override;
  virtual void allocate(void** pages, size_t num) noexcept override;
  using PageAllocator::deallocate;
  virtual void deallocate(void** pages, size_t num) noexcept override;

 private:
  struct Slot {
    ::std::vector<void*> buffer;
    ::std::vector<void*>::iterator next_page;
  };

  PageAllocator* _upstream {&SystemPageAllocator::instance()};
  size_t _batch_size {16};
  EnumerableThreadLocal<Slot> _cache;
};

// 统计分配量的分配器，实际动作交给上游完成
class CountingPageAllocator : public PageAllocator {
 public:
  void set_upstream(PageAllocator& upstream) noexcept;

  size_t allocated_page_num() noexcept;

  // 分配释放的内存块大小
  virtual size_t page_size() const noexcept override;
  // 分配释放接口
  virtual void* allocate() noexcept override;
  virtual void allocate(void** pages, size_t num) noexcept override;
  virtual void deallocate(void* page) noexcept override;
  virtual void deallocate(void** pages, size_t num) noexcept override;

 private:
  PageAllocator* _upstream {&SystemPageAllocator::instance()};
  ConcurrentAdder _allocate_page_num;
};

// 包装常用的分配器组合
// 由一个基础分配器和一个缓存分配器组合构成
class PageHeap : public PageAllocator {
 public:
  // 可默认构造，不支持拷贝和移动
  PageHeap() noexcept;
  PageHeap(PageHeap&&) = delete;
  PageHeap(const PageHeap&) = delete;
  PageHeap& operator=(PageHeap&&) = delete;
  PageHeap& operator=(const PageHeap&) = delete;

  // 设置页面大小和缓存容量
  // 需要在使用前完成调整
  void set_page_size(size_t page_size) noexcept;
  void set_free_page_capacity(size_t capacity) noexcept;

  // PageAllocator接口实现
  virtual size_t page_size() const noexcept override;
  using PageAllocator::allocate;
  virtual void allocate(void** pages, size_t num) noexcept override;
  using PageAllocator::deallocate;
  virtual void deallocate(void** pages, size_t num) noexcept override;

  // 已分配量
  size_t allocate_page_num() const noexcept;
  // 当前缓存量
  size_t free_page_num() const noexcept;
  // 缓存容量
  size_t free_page_capacity() const noexcept;
  // 命中统计量
  // sum为命中次数
  // num为总次数
  ConcurrentSummer::Summary cache_hit_summary() const noexcept;

  // 返回系统分配器结合默认缓存容量的PageHeap
  static PageHeap& system_page_heap() noexcept;

  // 不再推荐使用，采用默认构造加后设置方式更灵活
  PageHeap(size_t max_free_page_num) noexcept;
  PageHeap(size_t max_free_page_num, size_t page_size) noexcept;

 private:
  NewDeletePageAllocator _base_allocator;
  CachedPageAllocator _cached_allocator;
  ConcurrentAdder _allocate_page_num;
};

BABYLON_NAMESPACE_END

#include "babylon/unprotect.h"
