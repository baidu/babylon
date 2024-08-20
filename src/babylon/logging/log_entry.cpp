#include "babylon/logging/log_entry.h"

#include "babylon/protect.h"

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// LogStreamBuffer begin
void LogEntry::append_to_iovec(size_t page_size,
                               ::std::vector<struct ::iovec>& iov) noexcept {
  auto full_inline_size = INLINE_PAGE_CAPACITY * page_size;
  // 页面数量超过了可内联规模，需要考虑扩展页表
  if (size > full_inline_size) {
    // 先收集内联的整页
    pages_append_to_iovec(pages, full_inline_size - page_size, page_size, iov);
    // 再收集扩展页单链表中的内容
    page_table_append_to_iovec(*head, size - full_inline_size + page_size,
                               page_size, iov);
  } else {
    // 收集内联的非整页
    pages_append_to_iovec(pages, size, page_size, iov);
  }
}

void LogEntry::pages_append_to_iovec(
    char** pages, size_t size, size_t page_size,
    ::std::vector<struct ::iovec>& iov) noexcept {
  auto num = size / page_size;
  // 先收集靠前的整页
  for (size_t i = 0; i < num; ++i) {
    iov.emplace_back(::iovec {.iov_base = pages[i], .iov_len = page_size});
    size -= page_size;
  }
  // 收集位于尾部的最后一个非整页
  if (size > 0) {
    iov.emplace_back(::iovec {.iov_base = pages[num], .iov_len = size});
  }
}

void LogEntry::page_table_append_to_iovec(
    PageTable& table, size_t size, size_t page_size,
    ::std::vector<struct ::iovec>& iov) noexcept {
  auto full_table_size =
      (page_size - sizeof(PageTable)) / sizeof(char*) * page_size;

  auto table_ptr = &table;
  // 剩余内容足够填满一个页表内的所有页，完整收集
  while (size > full_table_size) {
    pages_append_to_iovec(table_ptr->pages, full_table_size, page_size, iov);
    iov.emplace_back(::iovec {.iov_base = table_ptr, .iov_len = 0});
    table_ptr = table_ptr->next;
    size -= full_table_size;
  }

  // 收集尾部可能不完整的页表
  if (size > 0) {
    pages_append_to_iovec(table_ptr->pages, size, page_size, iov);
    iov.emplace_back(::iovec {.iov_base = table_ptr, .iov_len = 0});
  }
}
// LogStreamBuffer end
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
    _log.size += static_cast<uintptr_t>(ptr - _sync_point);
    _sync_point = ptr;
  }
  return 0;
}

void LogStreamBuffer::overflow_page_table() noexcept {
  // 当前页表用尽，需要分配一个新的页表
  auto page_table =
      reinterpret_cast<LogEntry::PageTable*>(_page_allocator->allocate());
  page_table->next = nullptr;
  // 计算新页表的起止点
  auto pages = page_table->pages;
  auto pages_end = reinterpret_cast<char**>(
      reinterpret_cast<uintptr_t>(page_table) + _page_allocator->page_size());
  // 如果当前页表是日志对象内联页表
  if (_pages == _log.pages + LogEntry::INLINE_PAGE_CAPACITY) {
    // 转存最后一个页指针到新页表内，腾出页表的单链表头
    // 并将新页表挂载到头上
    *pages++ = reinterpret_cast<char*>(_log.head);
    _log.head = page_table;
  } else {
    // 从最后一个页表项推算出对应页表地址
    // 将新页表接在后面
    auto last_page_table = reinterpret_cast<LogEntry::PageTable*>(
        reinterpret_cast<uintptr_t>(_pages_end) - _page_allocator->page_size());
    last_page_table->next = page_table;
  }
  // 更新当前页表起止点
  _pages = pages;
  _pages_end = pages_end;
}
// LogStreamBuffer end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#include "babylon/unprotect.h"
