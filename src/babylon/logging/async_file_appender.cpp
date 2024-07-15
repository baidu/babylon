#include "babylon/logging/async_file_appender.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// AsyncFileAppender begin
AsyncFileAppender::~AsyncFileAppender() noexcept {
  close();
}

void AsyncFileAppender::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  _page_allocator = &page_allocator;
}

void AsyncFileAppender::set_queue_capacity(size_t queue_capacity) noexcept {
  _queue.reserve_and_clear(queue_capacity);
}

int AsyncFileAppender::initialize() noexcept {
  _write_thread = ::std::thread(&AsyncFileAppender::keep_writing, this);
  return 0;
}

void AsyncFileAppender::write(LogEntry& entry, FileObject* file) noexcept {
  _queue.push<true, false, false>([&](Item& target) {
    target.entry = entry;
    target.file = file;
  });
}

void AsyncFileAppender::discard(LogEntry& entry) noexcept {
  static thread_local ::std::vector<struct ::iovec> iov;
  static thread_local ::std::vector<void*> pages;

  entry.append_to_iovec(_page_allocator->page_size(), iov);
  for (auto& one_iov : iov) {
    pages.push_back(one_iov.iov_base);
  }

  _page_allocator->deallocate(pages.data(), pages.size());
  pages.clear();
  iov.clear();
}

int AsyncFileAppender::close() noexcept {
  if (_write_thread.joinable()) {
    _queue.push([](Item& target) {
      target.entry.size = 0;
      target.file = nullptr;
    });
    _write_thread.join();
  }
  return 0;
}

void AsyncFileAppender::keep_writing() noexcept {
  auto stop = false;
  // 由于writev限制了单次最大长度到UIO_MAXIOV，更大的batch并无意义
  // 另一方面ConcurrentBoundedQueue也限制了最大batch，合并取最严的限制
  auto batch = ::std::min<size_t>(UIO_MAXIOV, _queue.capacity());

  do {
    // 无论本轮是否有日志要写出，都检测一次文件描述符
    // 便于长期无日志输出时依然保持日志滚动
    auto poped = _queue.try_pop_n<false, false>(
        [&](Queue::Iterator iter, Queue::Iterator end) {
          while (iter < end) {
            auto& item = *iter++;
            if (ABSL_PREDICT_FALSE(item.entry.size == 0)) {
              stop = true;
              break;
            }
            auto& dest = destination(item.file);
            item.entry.append_to_iovec(_page_allocator->page_size(), dest.iov);
          }
        },
        batch);

    for (auto& dest : _destinations) {
      auto result = dest.file->check_and_get_file_descriptor();
      auto fd = ::std::get<0>(result);
      auto old_fd = ::std::get<1>(result);
      if (old_fd >= 0) {
        ::close(old_fd);
      }
      if (!dest.iov.empty()) {
        write_use_plain_writev(dest, fd);
      }
    }

    // 当一轮日志量过低时，拉长下一轮写入的周期
    // 尝试进行有效的批量写入
    if (poped < 100) {
      _backoff_us += 10;
      _backoff_us = ::std::min<size_t>(_backoff_us + 10, 100000);
      ::usleep(_backoff_us);
    } else if (poped >= batch) {
      // 获取到完整批量说明队列有积压
      // 缩短检测周期
      _backoff_us >>= 1;
    }
  } while (!stop);
}

AsyncFileAppender::Destination& AsyncFileAppender::destination(
    FileObject* file) noexcept {
  auto index = file->index();
  if (index != SIZE_MAX) {
    return _destinations[index];
  }

  file->set_index(_destinations.size());
  _destinations.emplace_back(Destination {.file = file});
  return _destinations.back();
}

void AsyncFileAppender::write_use_plain_writev(Destination& dest,
                                               int fd) noexcept {
  static thread_local ::std::vector<void*> pages;

  auto& iov = dest.iov;
  for (auto iter = iov.begin(); iter < iov.end();) {
    auto piov = &*iter;
    auto size = ::std::min<size_t>(IOV_MAX, iov.end() - iter);
    for (size_t i = 0; i < size; ++i) {
      pages.emplace_back(piov[i].iov_base);
    }
    ::writev(fd, piov, size);
    iter += size;
  }

  _page_allocator->deallocate(pages.data(), pages.size());
  pages.clear();
  iov.clear();
}
// AsyncFileAppender end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
