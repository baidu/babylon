#include "babylon/logging/log_stream.h"

#include "babylon/time.h"

#include "absl/time/clock.h" // absl::Now

#include <sys/syscall.h> // __NR_gettid
#include <unistd.h>      // ::syscall

#include <iostream> // std::cerr

BABYLON_NAMESPACE_BEGIN

class NullLogStream::Buffer : public ::std::streambuf {
 private:
  virtual int overflow(int ch) noexcept override {
    return ch;
  }
  virtual ::std::streamsize xsputn(const char*,
                                   ::std::streamsize count) noexcept override {
    return count;
  }
};

void LogStream::do_begin() noexcept {}
void LogStream::do_end() noexcept {}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wnull-dereference"
DefaultLogStream::DefaultLogStream() noexcept
    :
#if __clang__ || BABYLON_GCC_VERSION >= 50000
      LogStream {*reinterpret_cast<::std::streambuf*>(0)}
#else  // !__clang__ && BABYLON_GCC_VERSION < 50000
      LogStream(*reinterpret_cast<::std::streambuf*>(0))
#endif // !__clang__ && BABYLON_GCC_VERSION < 50000
{
  // Ensure std::cerr is initialized
  ::std::ios_base::Init();
  rdbuf(::std::cerr.rdbuf());
}
#pragma GCC diagnostic pop

::std::mutex& DefaultLogStream::mutex() noexcept {
  static ::std::mutex mutex;
  return mutex;
}

void DefaultLogStream::do_begin() noexcept {
  auto now_us = ::absl::ToUnixMicros(::absl::Now());
  time_t now_s = now_us / 1000 / 1000;
  time_t us = now_us % (1000 * 1000);
  struct ::tm time_struct;
  ::babylon::localtime(&now_s, &time_struct);
  mutex().lock();
  (*this) << severity();
  thread_local int tid = ::syscall(__NR_gettid);
  format(" %d-%02d-%02d %02d:%02d:%02d.%06d %d %.*s:%d] ",
         time_struct.tm_year + 1900, time_struct.tm_mon + 1,
         time_struct.tm_mday, time_struct.tm_hour, time_struct.tm_min,
         time_struct.tm_sec, us, tid, file().size(), file().data(), line());
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
      LogStream {buffer()} {
}
#else  // !__clang__ && BABYLON_GCC_VERSION < 50000
      LogStream(buffer()) {
}
#endif // !__clang__ && BABYLON_GCC_VERSION < 50000

NullLogStream::Buffer& NullLogStream::buffer() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static Buffer static_buffer;
#pragma GCC diagnostic pop
  return static_buffer;
}
// NullLogStream end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
