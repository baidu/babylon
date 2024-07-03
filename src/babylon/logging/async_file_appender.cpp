#include "babylon/logging/async_file_appender.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

BABYLON_NAMESPACE_BEGIN

void AsyncFileAppender::DirectBuffer::Deleter::operator()(
    DirectBuffer* buffer) noexcept {
  ::operator delete(buffer->data(), buffer->size(), buffer->alignment());
}

AsyncFileAppender::DirectBuffer::Ptr
AsyncFileAppender::DirectBuffer::create() noexcept {
  auto buffer = static_cast<DirectBuffer*>(
      ::operator new(DirectBuffer::size(), DirectBuffer::alignment()));
  ::madvise(buffer->data(), buffer->size(), MADV_HUGEPAGE);
  ::memset(buffer->data(), 0, buffer->size());
  Ptr ptr {buffer, Deleter()};
  return ptr;
}

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
  _direct_buffer = DirectBuffer::create();

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
  _direct_buffer.reset();
  return 0;
}

void AsyncFileAppender::prepare_destination(int fd,
                                            Destination& dest) noexcept {
  dest.fd = fd;
  dest.fd_index = -1;
  auto flags = ::fcntl(dest.fd, F_GETFL);
  if (flags < 0) {
    flags = 0;
  }

  dest.direct = static_cast<bool>(flags & O_DIRECT);
  if (!dest.direct) {
    dest.write = &AsyncFileAppender::write_use_plain_writev;
    return;
  }

  dest.write = &AsyncFileAppender::write_use_direct_pwrite;
  struct ::stat st;
  auto ret = ::fstat(dest.fd, &st);
  auto file_size = st.st_size;
  if (ret < 0) {
    file_size = 0;
  }

  dest.offset = file_size / 512 * 512;
  auto last_sector_size = file_size % 512;
  if (last_sector_size > 0) {
    auto bytes = ::pread(dest.fd, _direct_buffer->data(), 512, 0);
    if (bytes > 0) {
      auto page = _page_allocator->allocate();
      ::memcpy(page, _direct_buffer->data(), bytes);
      dest.iov.emplace(
          dest.iov.begin(),
          ::iovec {.iov_base = page, .iov_len = static_cast<size_t>(bytes)});
    }
  }
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
            dest.synced = false;
          }
        },
        batch);

    for (auto& dest : _destinations) {
      auto result = dest.file->check_and_get_file_descriptor();
      auto fd = ::std::get<0>(result);
      auto old_fd = ::std::get<1>(result);
      ::std::cerr << "index " << dest.file->index() << " fd " << fd
                  << " old_fd " << old_fd << ::std::endl;
      if (old_fd >= 0) {
        abort();
      }
      if (dest.fd < 0) {
        prepare_destination(fd, dest);
      }
    }

    for (auto& dest : _destinations) {
      if (!dest.iov.empty()) {
        (this->*(dest.write))(dest);
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

size_t AsyncFileAppender::pwrite_all(int fd, const char* buffer, size_t size,
                                     off_t offset) noexcept {
  auto total = 0;
  auto remain = size;
  while (remain > 0) {
    auto written = ::pwrite(fd, buffer, remain, offset);
    if (written < 0) {
      return total;
    }
    buffer += written;
    remain -= written;
    offset += written;
    total += written;
  }
  return total;
}

void AsyncFileAppender::write_use_plain_writev(Destination& dest) noexcept {
  static thread_local ::std::vector<void*> pages;

  auto fd = dest.fd;
  auto& iov = dest.iov;
  for (auto iter = iov.begin(); iter < iov.end();) {
    auto piov = &*iter;
    auto size = ::std::min<size_t>(IOV_MAX, iov.end() - iter);
    auto bytes = 0;
    for (size_t i = 0; i < size; ++i) {
      pages.emplace_back(piov[i].iov_base);
      bytes += piov[i].iov_len;
    }
    auto written = ::writev(fd, piov, size);
    ::std::cerr << "write_use_plain_writev writev to fd " << fd << " ret "
                << written << ::std::endl;
    if (bytes == written) {
      iter += size;
      continue;
    }
    abort();
  }

  _page_allocator->deallocate(pages.data(), pages.size());
  pages.clear();
  iov.clear();
  dest.synced = true;
}

void AsyncFileAppender::write_use_direct_pwrite(Destination& dest) noexcept {
  if (dest.synced) {
    return;
  }

  static thread_local ::std::vector<void*> pages;
  pages.clear();

  char* buffer = _direct_buffer->data();
  size_t size = _direct_buffer->size();
  auto& iov = dest.iov;

  for (auto& one_iov : iov) {
    ::std::cerr << "write_use_direct_pwrite process iov " << one_iov.iov_base
                << " size " << one_iov.iov_len << ::std::endl;
    if (one_iov.iov_len <= size) {
      ::memcpy(buffer, one_iov.iov_base, one_iov.iov_len);
      buffer += one_iov.iov_len;
      size -= one_iov.iov_len;
    } else {
      auto src = static_cast<char*>(one_iov.iov_base);
      auto remain = one_iov.iov_len;
      ::memcpy(buffer, src, size);
      auto written = pwrite_all(dest.fd, _direct_buffer->data(),
                                _direct_buffer->size(), dest.offset);
      dest.offset += written;
      buffer = _direct_buffer->data();
      src += size;
      remain -= size;
      ::memcpy(buffer, src, remain);
      buffer += remain;
      size = _direct_buffer->size() - remain;
    }
    pages.emplace_back(one_iov.iov_base);
  }

  auto fill_bytes = size % 512;
  if (fill_bytes > 0) {
    ::memset(buffer, '\0', fill_bytes);
    size -= fill_bytes;
  }

  auto write_bytes = _direct_buffer->size() - size;
  auto content_bytes = write_bytes - fill_bytes;
  auto written =
      pwrite_all(dest.fd, _direct_buffer->data(), write_bytes, dest.offset);
  if (written < content_bytes) {
    fill_bytes = 0;
  }
  dest.offset += written;

  if (fill_bytes > 0) {
    dest.offset -= 512;
    auto keep_bytes = 512 - fill_bytes;
    iov.resize(1);
    auto& first_iov = iov[0];
    first_iov.iov_base = pages.back();
    pages.pop_back();
    ::memcpy(first_iov.iov_base, buffer - keep_bytes, keep_bytes);
    first_iov.iov_len = keep_bytes;
    ::std::cerr << "copy_to_direct_buffer keep " << keep_bytes << ::std::endl;
  }
  _page_allocator->deallocate(pages.data(), pages.size());
  dest.synced = true;
}
// AsyncFileAppender end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
