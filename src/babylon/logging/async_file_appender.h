#pragma once

#include "babylon/concurrent/bounded_queue.h" // babylon::ConcurrentBoundedQueue
#include "babylon/logging/file_object.h"      // babylon::PageAllocator
#include "babylon/logging/log_entry.h"        // babylon::LogEntry
#include "babylon/reusable/page_allocator.h"  // babylon::PageAllocator

#include <sys/uio.h> // ::iovec

#include <streambuf> // std::streambuf
#include <thread>    // std::thread
#include <vector>    // std::vector

BABYLON_NAMESPACE_BEGIN

// 实现日志异步追加写入文件的组件
// 对外提供的写入接口将日志存入队列中即返回
// 之后由独立的异步线程完成到文件的写入动作
class AsyncFileAppender {
 public:
  // 析构时自动关闭
  ~AsyncFileAppender() noexcept;

  // 设置日志文件对象，内存池，以及队列容量等参数
  void set_page_allocator(PageAllocator& page_allocator) noexcept;
  void set_queue_capacity(size_t queue_capacity) noexcept;
  void set_direct_buffer_size(size_t size) noexcept;

  // 启动异步线程，进入工作状态
  int initialize() noexcept;

  // 获取内存池，日志对象需要在这个内存池上分配
  inline PageAllocator& page_allocator() noexcept;
  // 提交一个构造好的日志对象
  void write(LogEntry& log, FileObject* file_object) noexcept;
  // 放弃一个构造好的日志对象
  void discard(LogEntry& log) noexcept;
  // 返回当前待写入的日志对象数目
  inline size_t pending_size() const noexcept;

  // 等待已入队日志写入完成后关闭异步线程
  int close() noexcept;

 private:
  struct Item {
    LogEntry entry;
    FileObject* file;
  };
  using Queue = ConcurrentBoundedQueue<Item>;

  struct Destination {
    FileObject* file {nullptr};
    ::std::vector<struct ::iovec> iov {};
  };

  static void writev_all(int fd, const ::std::vector<struct ::iovec>& iov,
                         size_t bytes) noexcept;

  void keep_writing() noexcept;
  Destination& destination(FileObject* object) noexcept;

  void write_use_plain_writev(Destination& dest, int fd) noexcept;

  Queue _queue {1024};
  PageAllocator* _page_allocator {&SystemPageAllocator::instance()};
  ::std::thread _write_thread;
  size_t _backoff_us {0};
  ::std::vector<Destination> _destinations;
};

////////////////////////////////////////////////////////////////////////////////
// AsyncFileAppender begin
inline PageAllocator& AsyncFileAppender::page_allocator() noexcept {
  return *_page_allocator;
}

inline size_t AsyncFileAppender::pending_size() const noexcept {
  return _queue.size();
}
// AsyncFileAppender end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
