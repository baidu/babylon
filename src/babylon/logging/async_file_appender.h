#pragma once

#include "babylon/concurrent/bounded_queue.h" // babylon::ConcurrentBoundedQueue
#include "babylon/reusable/page_allocator.h"  // babylon::PageAllocator

#include <sys/uio.h> // ::iovec

#include <streambuf> // std::streambuf
#include <thread>    // std::thread
#include <vector>    // std::vector

BABYLON_NAMESPACE_BEGIN

// 解耦异步日志机制中的文件操作和异步传输
// 对接具体日志框架时，文件操作层需要适配对应框架机制
// 内部完成截断/滚动/磁盘容量控制等功能
// 将这部分独立出来有助于实现日志框架无关的异步传输层
class FileObject {
 public:
  virtual ~FileObject() noexcept = 0;

  // 核心功能函数，异步传输层在每次需要写出日志时都会调用此函数
  // 并需要接口返回一个可用的文件描述符，最终日志会通过这个文件来输出
  // 如果需要进行文件滚动等操作，内部完成后返回最终可用的描述符
  virtual int check_and_get_file_descriptor() noexcept = 0;
};

// 指向标准错误流文件的包装
class StderrFileObject : public FileObject {
 public:
  static StderrFileObject& instance() noexcept;

  virtual int check_and_get_file_descriptor() noexcept override;
};

// 表达一条日志的结构，实际内容分页存储管理
// 由于需要做缓存行隔离，因此结构本身可以内联存储少量页指针
// 当日志页超过可内联容量时，展开单链表组织的独立页表进行管理
struct Log {
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

  // 元信息只记录一个总长度，总长度确定后
  // 实际的页数目和组织方式也可以唯一确定
  size_t size;

  // 为了提升内联容量，页指针数组最后一个元素和页表头指针是复用的
  // 容量减一为了方便形成结构
  char* pages[INLINE_PAGE_CAPACITY - 1];
  PageTable* head;
};

// 支持渐进构造一行日志
// 采用std::streambuf接口实现，便于集成到std::ostream格式化机制
//
// 极简用法
// LogStreamBuffer streambuf;
// streambuf.set_page_allocator(allocator);
// loop:
//   streambuf.begin();
//   streambuf.sputn(..., sizeof(...));
//   streambuf.sputn(..., sizeof(...));
//   appender.write(streambuf.end());
class LogStreamBuffer : public ::std::streambuf {
 public:
  inline void set_page_allocator(PageAllocator& page_allocator) noexcept;

  inline void begin() noexcept;
  inline Log& end() noexcept;

  inline void begin(PageAllocator& page_allocator) noexcept;

 private:
  virtual int overflow(int ch) noexcept override;
  virtual int sync() noexcept override;

  void overflow_page_table() noexcept;

  Log _log;
  PageAllocator* _page_allocator;
  char** _pages;
  char** _pages_end;
  char* _sync_point;
};

// 实现日志异步追加写入文件的组件
// 对外提供的写入接口将日志存入队列中即返回
// 之后由独立的异步线程完成到文件的写入动作
class AsyncFileAppender {
 public:
  // 析构时自动关闭
  ~AsyncFileAppender() noexcept;

  // 设置日志文件对象，内存池，以及队列容量等参数
  void set_file_object(FileObject& file_object) noexcept;
  void set_page_allocator(PageAllocator& page_allocator) noexcept;
  void set_queue_capacity(size_t queue_capacity) noexcept;

  // 启动异步线程，进入工作状态
  int initialize() noexcept;

  // 获取内存池，日志对象需要在这个内存池上分配
  inline PageAllocator& page_allocator() noexcept;
  // 提交一个构造好的日志对象
  void write(Log& log) noexcept;
  // 放弃一个构造好的日志对象
  void discard(Log& log) noexcept;
  // 返回当前待写入的日志对象数目
  inline size_t pending_size() const noexcept;

  // 等待已入队日志写入完成后关闭异步线程
  int close() noexcept;

 private:
  using Queue = ConcurrentBoundedQueue<Log>;

  constexpr static size_t INLINE_PAGE_CAPACITY = Log::INLINE_PAGE_CAPACITY;

  void keep_writing() noexcept;
  bool collect_log_pages(Log& log, ::std::vector<::iovec>& writev_payloads,
                         ::std::vector<void*>& pages_to_release) noexcept;
  void collect_log_pages(Log::PageTable& page_table, size_t size,
                         ::std::vector<::iovec>& writev_payloads,
                         ::std::vector<void*>& pages_to_release) noexcept;
  void collect_pages(char** pages, size_t byte_size,
                     ::std::vector<::iovec>& writev_payloads,
                     ::std::vector<void*>& pages_to_release) noexcept;
  void collect_full_pages(char** pages, size_t num,
                          ::std::vector<::iovec>& writev_payloads,
                          ::std::vector<void*>& pages_to_release) noexcept;
  void write_to_fd(::std::vector<::iovec>& writev_payloads, int fd) noexcept;

  Queue _queue {1024};
  PageAllocator* _page_allocator {&SystemPageAllocator::instance()};
  FileObject* _file_object {&StderrFileObject::instance()};
  size_t _extend_page_capacity {0};
  size_t _backoff_us {0};
  ::std::thread _write_thread;
};

inline void LogStreamBuffer::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  _page_allocator = &page_allocator;
}

inline void LogStreamBuffer::begin() noexcept {
  auto page = reinterpret_cast<char*>(_page_allocator->allocate());
  _log.size = 0;
  _log.pages[0] = page;
  _pages = _log.pages + 1;
  _pages_end = _log.pages + Log::INLINE_PAGE_CAPACITY;
  _sync_point = page;
  setp(page, page + _page_allocator->page_size());
}

inline void LogStreamBuffer::begin(PageAllocator& page_allocator) noexcept {
  set_page_allocator(page_allocator);
  begin();
}

inline Log& LogStreamBuffer::end() noexcept {
  sync();
  return _log;
}

inline PageAllocator& AsyncFileAppender::page_allocator() noexcept {
  return *_page_allocator;
}

inline size_t AsyncFileAppender::pending_size() const noexcept {
  return _queue.size();
}

BABYLON_NAMESPACE_END
