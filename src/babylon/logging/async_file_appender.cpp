#include "babylon/logging/async_file_appender.h"

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// FileObject begin
FileObject::~FileObject() noexcept {}
// FileObject end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// StderrFileObject begin
StderrFileObject& StderrFileObject::instance() noexcept {
  static StderrFileObject object;
  return object;
}

int StderrFileObject::check_and_get_file_descriptor() noexcept {
  return STDERR_FILENO;
}
// StderrFileObject end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// LogStreamBuffer begin
int LogStreamBuffer::overflow(int ch) noexcept {
  // 先同步字节数
  sync();
  // 分配一个新页，设置到缓冲区
  auto page = reinterpret_cast<char*>(_page_allocator->allocate());
  if (ABSL_PREDICT_FALSE(_pages == _pages_end)) {
    overflow_page_table();
  }
  *_pages++ = page;
  _sync_point = page;
  setp(page, page + _page_allocator->page_size());
  return sputc(ch);
}

int LogStreamBuffer::sync() noexcept {
  auto ptr = pptr();
  if (ptr > _sync_point) {
    _log.size += ptr - _sync_point;
    _sync_point = ptr;
  }
  return 0;
}

void LogStreamBuffer::overflow_page_table() noexcept {
  // 当前页表用尽，需要分配一个新的页表
  auto page_table =
      reinterpret_cast<Log::PageTable*>(_page_allocator->allocate());
  page_table->next = nullptr;
  // 计算新页表的起止点
  auto pages = page_table->pages;
  auto pages_end = reinterpret_cast<char**>(
      reinterpret_cast<uintptr_t>(page_table) + _page_allocator->page_size());
  // 如果当前页表是日志对象内联页表
  if (_pages == _log.pages + Log::INLINE_PAGE_CAPACITY) {
    // 转存最后一个页指针到新页表内，腾出页表的单链表头
    // 并将新页表挂载到头上
    *pages++ = reinterpret_cast<char*>(_log.head);
    _log.head = page_table;
  } else {
    // 从最后一个页表项推算出对应页表地址
    // 将新页表接在后面
    auto last_page_table = reinterpret_cast<Log::PageTable*>(
        reinterpret_cast<uintptr_t>(_pages_end) - _page_allocator->page_size());
    last_page_table->next = page_table;
  }
  // 更新当前页表起止点
  _pages = pages;
  _pages_end = pages_end;
}
// LogStreamBuffer end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// AsyncFileAppender begin
AsyncFileAppender::~AsyncFileAppender() noexcept {
  close();
}

void AsyncFileAppender::set_file_object(FileObject& file_object) noexcept {
  _file_object = &file_object;
}

void AsyncFileAppender::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  _page_allocator = &page_allocator;
}

void AsyncFileAppender::set_queue_capacity(size_t queue_capacity) noexcept {
  _queue.reserve_and_clear(queue_capacity);
}

int AsyncFileAppender::initialize() noexcept {
  // 根据页大小，计算出页表容量
  _extend_page_capacity =
      (_page_allocator->page_size() - sizeof(Log::PageTable)) / sizeof(char*);
  _write_thread = ::std::thread(&AsyncFileAppender::keep_writing, this);
  return 0;
}

void AsyncFileAppender::write(Log& log) noexcept {
  _queue.push<true, false, false>([&](Log& target) {
    target = log;
  });
}

void AsyncFileAppender::discard(Log& log) noexcept {
  thread_local ::std::vector<::iovec> writev_payloads;
  thread_local ::std::vector<void*> pages_to_release;
  writev_payloads.clear();
  pages_to_release.clear();
  collect_log_pages(log, writev_payloads, pages_to_release);
  _page_allocator->deallocate(pages_to_release.data(), pages_to_release.size());
}

int AsyncFileAppender::close() noexcept {
  if (_write_thread.joinable()) {
    _queue.push([](Log& log) {
      log.size = 0;
    });
    _write_thread.join();
  }
  return 0;
}

void AsyncFileAppender::keep_writing() noexcept {
  ::std::vector<::iovec> writev_payloads;
  ::std::vector<void*> pages_to_release;
  auto stop = false;
  // 由于writev限制了单次最大长度到UIO_MAXIOV，更大的batch并无意义
  // 另一方面ConcurrentBoundedQueue也限制了最大batch，合并取最严的限制
  auto batch = ::std::min<size_t>(UIO_MAXIOV, _queue.capacity());

  do {
    writev_payloads.clear();
    pages_to_release.clear();

    // 无论本轮是否有日志要写出，都检测一次文件描述符
    // 便于长期无日志输出时依然保持日志滚动
    auto fd = _file_object->check_and_get_file_descriptor();
    auto poped = _queue.try_pop_n<false, false>(
        [&](Queue::Iterator iter, Queue::Iterator end) {
          while (iter < end) {
            stop =
                collect_log_pages(*iter++, writev_payloads, pages_to_release);
          }
        },
        batch);

    if (writev_payloads.size() > 0) {
      write_to_fd(writev_payloads, fd);
      _page_allocator->deallocate(pages_to_release.data(),
                                  pages_to_release.size());
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

bool AsyncFileAppender::collect_log_pages(
    Log& log, ::std::vector<::iovec>& writev_payloads,
    ::std::vector<void*>& pages_to_release) noexcept {
  if (ABSL_PREDICT_FALSE(log.size == 0)) {
    return true;
  }

  auto page_size = _page_allocator->page_size();
  size_t data_page_num = (log.size + page_size - 1) / page_size;
  // 页面数量超过了可内联规模，需要考虑扩展页表
  if (data_page_num > Log::INLINE_PAGE_CAPACITY) {
    // 先收集内联的整页
    collect_full_pages(log.pages, Log::INLINE_PAGE_CAPACITY - 1,
                       writev_payloads, pages_to_release);
    // 再收集扩展页单链表中的内容
    auto remain = log.size - ((Log::INLINE_PAGE_CAPACITY - 1) * page_size);
    collect_log_pages(*log.head, remain, writev_payloads, pages_to_release);
    return false;
  }

  collect_pages(log.pages, log.size, writev_payloads, pages_to_release);
  return false;
}

void AsyncFileAppender::collect_log_pages(
    Log::PageTable& extend_log, size_t size, ::std::vector<::iovec>& payloads,
    ::std::vector<void*>& pages) noexcept {
  auto extend_page_capacity = _extend_page_capacity;
  auto extend_capacity = extend_page_capacity * _page_allocator->page_size();

  auto remain = size;
  auto extend_log_ptr = &extend_log;
  // 剩余内容足够填满一个页表内的所有页，完整收集
  while (remain >= extend_capacity) {
    collect_full_pages(extend_log_ptr->pages, extend_page_capacity, payloads,
                       pages);
    pages.emplace_back(extend_log_ptr);
    extend_log_ptr = extend_log_ptr->next;
    remain -= extend_capacity;
  }
  // 收集尾部可能不完整的页表
  if (remain > 0) {
    collect_pages(extend_log_ptr->pages, remain, payloads, pages);
    pages.emplace_back(extend_log_ptr);
  }
}

void AsyncFileAppender::collect_full_pages(
    char** pages, size_t num, ::std::vector<::iovec>& writev_payloads,
    ::std::vector<void*>& pages_to_release) noexcept {
  auto page_size = _page_allocator->page_size();
  for (size_t i = 0; i < num; ++i) {
    auto page = pages[i];
    writev_payloads.emplace_back(
        ::iovec {.iov_base = page, .iov_len = page_size});
    pages_to_release.emplace_back(page);
  }
}

void AsyncFileAppender::collect_pages(
    char** pages, size_t byte_size, ::std::vector<::iovec>& writev_payloads,
    ::std::vector<void*>& pages_to_release) noexcept {
  auto page_size = _page_allocator->page_size();
  auto page_ptr = pages;
  auto remain = byte_size;
  // 先收集靠前的整页
  while (remain >= page_size) {
    auto page = *page_ptr++;
    writev_payloads.emplace_back(
        ::iovec {.iov_base = page, .iov_len = page_size});
    pages_to_release.emplace_back(page);
    remain -= page_size;
  }
  // 收集位于尾部的最后一个非整页
  if (remain > 0) {
    auto page = *page_ptr;
    writev_payloads.emplace_back(::iovec {.iov_base = page, .iov_len = remain});
    pages_to_release.emplace_back(page);
  }
}

void AsyncFileAppender::write_to_fd(::std::vector<::iovec>& writev_payloads,
                                    int fd) noexcept {
  size_t begin_index = 0;
  do {
    auto size =
        ::std::min<size_t>(UIO_MAXIOV, writev_payloads.size() - begin_index);
    auto written = ::writev(fd, &writev_payloads[begin_index], size);
    (void)written;
    begin_index += size;
  } while (begin_index < writev_payloads.size());
}
// AsyncFileAppender end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
