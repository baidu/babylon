#include "babylon/logging/log_stream.h"

#include "babylon/time.h"

#include "absl/time/clock.h" // absl::GetCurrentTimeNanos

#include <iostream> // std::cerr

BABYLON_NAMESPACE_BEGIN

void LogStream::do_begin() noexcept {}
void LogStream::do_end() noexcept {}

DefaultLogStream::DefaultLogStream() noexcept
    :
#if __clang__ || BABYLON_GCC_VERSION >= 50000
      LogStream {*::std::cerr.rdbuf()} {
}
#else  // !__clang__ && BABYLON_GCC_VERSION < 50000
      LogStream(*::std::cerr.rdbuf()) {
}
#endif // !__clang__ && BABYLON_GCC_VERSION < 50000

::std::mutex& DefaultLogStream::mutex() noexcept {
  static ::std::mutex mutex;
  return mutex;
}

void DefaultLogStream::do_begin() noexcept {
  auto now_ns = ::absl::GetCurrentTimeNanos();
  time_t now = now_ns / 1000000000L;
  time_t ms = now_ns % 1000000000L / 1000;
  struct ::tm time_struct;
  ::babylon::localtime(&now, &time_struct);
  mutex().lock();
  (*this) << severity();
  format(" %d-%02d-%02d %02d:%02d:%02d.%06d %.*s:%d] ",
         time_struct.tm_year + 1900, time_struct.tm_mon + 1,
         time_struct.tm_mday, time_struct.tm_hour, time_struct.tm_min,
         time_struct.tm_sec, ms, file().size(), file().data(), line());
}

void DefaultLogStream::do_end() noexcept {
  (*this) << '\n';
  mutex().unlock();
}

////////////////////////////////////////////////////////////////////////////////
// NullLogStream begin
NullLogStream::NullLogStream() noexcept
    :
#if __clang__ || BABYLON_GCC_VERSION >= 50000
      LogStream {*static_cast<::std::streambuf*>(nullptr)} {
}
#else  // !__clang__ && BABYLON_GCC_VERSION < 50000
      LogStream(*static_cast<::std::streambuf*>(nullptr)) {
}
#endif // !__clang__ && BABYLON_GCC_VERSION < 50000
// NullLogStream end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
