#pragma once

#include "babylon/reusable/page_allocator.h" // babylon::PageAllocator

#include <sys/uio.h> // ::iovec

BABYLON_NAMESPACE_BEGIN

// 表达一条日志的结构，实际内容分页存储管理
// 由于需要做缓存行隔离，因此结构本身可以内联存储少量页指针
// 当日志页超过可内联容量时，展开单链表组织的独立页表进行管理
class LogEntry {
 public:
  // 先计算缓存行对齐模式下日志结构的最大尺寸
  constexpr static size_t MAX_INLINE_SIZE =
      BABYLON_CACHELINE_SIZE -
      ((sizeof(Futex<SchedInterface>) + alignof(void*) - 1) & ~alignof(void*));
  // 根据尺寸算出可以支持多少内联页
  constexpr static size_t INLINE_PAGE_CAPACITY =
      (MAX_INLINE_SIZE - sizeof(size_t)) / sizeof(void*);

  // 通过单链表组织的页表结构
  // 页表结构自身也采用页大小来分配，因此实际的页指针数组容量
  // 和页大小相关，需要动态确定，这里设置一个假的零容量
  struct PageTable {
    PageTable* next;
    char* pages[0];
  };

  void append_to_iovec(size_t page_size,
                       ::std::vector<struct ::iovec>& iov) noexcept;

 private:
  static void pages_append_to_iovec(
      char** pages, size_t size, size_t page_size,
      ::std::vector<struct ::iovec>& iov) noexcept;
  static void page_table_append_to_iovec(
      PageTable& page_table, size_t size, size_t page_size,
      ::std::vector<struct ::iovec>& iov) noexcept;

 public:
  // 元信息只记录一个总长度，总长度确定后
  // 实际的页数目和组织方式也可以唯一确定
  size_t size;

  // 为了提升内联容量，页指针数组最后一个元素和页表头指针是复用的
  // 容量减一为了方便形成结构
  char* pages[INLINE_PAGE_CAPACITY - 1];
  PageTable* head;
};

// 支持渐进式构造一条日志
// 采用std::streambuf接口实现，便于集成到std::ostream格式化机制
//
// 极简用法
// LogStreamBuffer streambuf;
// streambuf.set_page_allocator(allocator);
// loop:
//   streambuf.begin();
//   streambuf.sputn(..., sizeof(...));
//   streambuf.sputn(..., sizeof(...));
//   appender.write(streambuf.end(), file_object);
class LogStreamBuffer : public ::std::streambuf {
 public:
  inline void set_page_allocator(PageAllocator& page_allocator) noexcept;

  inline void begin() noexcept;
  inline LogEntry& end() noexcept;

 private:
  virtual int overflow(int ch) noexcept override;
  virtual int sync() noexcept override;

  void overflow_page_table() noexcept;

  LogEntry _log;
  PageAllocator* _page_allocator;
  char** _pages;
  char** _pages_end;
  char* _sync_point;
};

////////////////////////////////////////////////////////////////////////////////
// LogStreamBuffer begin
inline void LogStreamBuffer::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  _page_allocator = &page_allocator;
}

inline void LogStreamBuffer::begin() noexcept {
  _log.size = 0;
  _pages = _log.pages;
  _pages_end = _log.pages + LogEntry::INLINE_PAGE_CAPACITY;
  _sync_point = nullptr;
  setp(nullptr, nullptr);
}

inline LogEntry& LogStreamBuffer::end() noexcept {
  sync();
  return _log;
}
// LogStreamBuffer end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
