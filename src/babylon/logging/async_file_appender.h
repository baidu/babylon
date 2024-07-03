#pragma once

#include "babylon/concurrent/bounded_queue.h" // babylon::ConcurrentBoundedQueue
#include "babylon/logging/file_object.h"      // babylon::PageAllocator
#include "babylon/logging/log_entry.h"        // babylon::LogEntry
#include "babylon/logging/logger.h"           // babylon::LoggerBuilder
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

  struct DirectBuffer {
    struct Deleter {
      void operator()(DirectBuffer* buffer) noexcept;

      friend class ::std::unique_ptr<DirectBuffer, Deleter>;
    };
    using Ptr = ::std::unique_ptr<DirectBuffer, Deleter>;

    static Ptr create() noexcept;

    inline char* data() noexcept;
    inline static constexpr size_t size() noexcept;
    inline static constexpr ::std::align_val_t alignment() noexcept;

    friend class ::std::unique_ptr<DirectBuffer, Deleter>;
  };

 private:
  struct Item {
    LogEntry entry;
    FileObject* file;
  };
  using Queue = ConcurrentBoundedQueue<Item>;

  struct Destination;
  using Writer = void (AsyncFileAppender::*)(Destination&) noexcept;
  struct Destination {
    FileObject* file;
    ::std::vector<struct ::iovec> iov {};
    int fd {-1};
    int fd_index {-1};
    bool direct {false};
    bool synced {false};
    size_t offset {0};
    Writer write {nullptr};
  };

  constexpr static size_t INLINE_PAGE_CAPACITY = LogEntry::INLINE_PAGE_CAPACITY;

  static size_t pwrite_all(int fd, const char* buffer, size_t size,
                           off_t offset) noexcept;

  void prepare_destination(int fd, Destination& dest) noexcept;
  void keep_writing() noexcept;
  Destination& destination(FileObject* object) noexcept;

  void write_use_plain_writev(Destination& dest) noexcept;
  void write_use_direct_pwrite(Destination& dest) noexcept;

  Queue _queue {1024};
  PageAllocator* _page_allocator {&SystemPageAllocator::instance()};
  ::std::thread _write_thread;
  size_t _backoff_us {0};
  ::std::vector<Destination> _destinations;

  DirectBuffer::Ptr _direct_buffer;
};

////////////////////////////////////////////////////////////////////////////////
// AsyncFileAppender::DirectBuffer begin
inline char* AsyncFileAppender::DirectBuffer::data() noexcept {
  return reinterpret_cast<char*>(this);
}

inline constexpr size_t AsyncFileAppender::DirectBuffer::size() noexcept {
  // return 2UL << 20;
  return 2048;
}

inline constexpr ::std::align_val_t
AsyncFileAppender::DirectBuffer::alignment() noexcept {
  // return ::std::align_val_t(2UL << 20);
  return ::std::align_val_t(2048);
}
// AsyncFileAppender::DirectBuffer end
////////////////////////////////////////////////////////////////////////////////

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
