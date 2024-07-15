#include "babylon/logging/async_log_stream.h"

#include "babylon/time.h" // localtime

#include <sys/syscall.h> // __NR_gettid
#include <unistd.h>      // ::syscall

BABYLON_NAMESPACE_BEGIN

LoggerBuilder::Creator AsyncLogStream::creator(
    AsyncFileAppender& appender, FileObject& file_object) noexcept {
  return creator(appender, file_object, default_header_formatter);
}

LoggerBuilder::Creator AsyncLogStream::creator(
    AsyncFileAppender& appender, FileObject& file_object,
    HeaderFormatter formatter) noexcept {
  return [&, formatter]() {
    return ::std::unique_ptr<AsyncLogStream> {
        new AsyncLogStream {&appender, &file_object, formatter}, {}};
  };
}

void AsyncLogStream::default_header_formatter(AsyncLogStream& ls) noexcept {
  auto now = ::absl::ToTimeval(::absl::Now());
  struct ::tm time_struct;
  StringView severity_name = ls.severity();
  ::babylon::localtime(&now.tv_sec, &time_struct);
  thread_local int tid = ::syscall(__NR_gettid);
  ls.format("%.*s %d-%02d-%02d %02d:%02d:%02d.%06d %d %.*s:%d] ",
            severity_name.size(), severity_name.data(),
            time_struct.tm_year + 1900, time_struct.tm_mon + 1,
            time_struct.tm_mday, time_struct.tm_hour, time_struct.tm_min,
            time_struct.tm_sec, now.tv_usec, tid, ls.file().size(),
            ls.file().data(), ls.line());
}

AsyncLogStream::AsyncLogStream(AsyncFileAppender* appender,
                               FileObject* file_object,
                               HeaderFormatter formatter) noexcept
    : LogStream {_buffer},
      _appender {appender},
      _file_object {file_object},
      _formatter {formatter} {}

void AsyncLogStream::do_begin() noexcept {
  _buffer.set_page_allocator(_appender->page_allocator());
  _buffer.begin();
  _formatter(*this);
}

void AsyncLogStream::do_end() noexcept {
  _buffer.sputc('\n');
  auto& log_entry = _buffer.end();
  _appender->write(log_entry, _file_object);
}

BABYLON_NAMESPACE_END
