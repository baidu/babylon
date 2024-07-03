#pragma once

#include "babylon/logging/log_stream.h"

BABYLON_NAMESPACE_BEGIN

inline void LogStream::set_severity(LogSeverity severity) noexcept {
  _severity = severity;
}

inline LogSeverity LogStream::severity() const noexcept {
  return _severity;
}

inline void LogStream::set_file(StringView file) noexcept {
  _file = file;
}

inline StringView LogStream::file() const noexcept {
  return _file;
}

inline void LogStream::set_line(int line) noexcept {
  _line = line;
}

inline int LogStream::line() const noexcept {
  return _line;
}

template <typename... Args>
inline LogStream& LogStream::begin(const Args&... args) noexcept {
  if (ABSL_PREDICT_FALSE(++_depth != 1)) {
    return *this;
  }
  if (!_noflush) {
    do_begin();
    write_objects(args...);
  } else {
    _noflush = false;
  }
  return *this;
}

inline LogStream& LogStream::noflush() noexcept {
  if (ABSL_PREDICT_FALSE(_depth != 1)) {
    return *this;
  }
  _noflush = true;
  return *this;
}

inline LogStream& LogStream::end() noexcept {
  if (ABSL_PREDICT_FALSE(--_depth > 0)) {
    return *this;
  }
  if (!_noflush) {
    do_end();
    clear();
  }
  return *this;
}

inline LogStream& LogStream::write(const char* data, size_t size) noexcept {
  rdbuf()->sputn(data, size);
  return *this;
}

inline LogStream& LogStream::write(char c) noexcept {
  rdbuf()->sputc(c);
  return *this;
}

template <typename... Args>
inline LogStream& LogStream::format(const ::absl::FormatSpec<Args...>& format,
                                    const Args&... args) noexcept {
  return do_absl_format(this, format, args...);
}

template <
    typename T, typename... Args,
    typename ::std::enable_if<
        ::std::is_convertible<T*, ::absl::FormatRawSink>::value, int>::type>
inline LogStream& LogStream::do_absl_format(
    T* ls, const ::absl::FormatSpec<Args...>& format,
    const Args&... args) noexcept {
  ::absl::Format(ls, format, args...);
  return *ls;
}

template <
    typename T, typename... Args,
    typename ::std::enable_if<
        !::std::is_convertible<T*, ::absl::FormatRawSink>::value, int>::type>
inline LogStream& LogStream::do_absl_format(
    T* ls, const ::absl::FormatSpec<Args...>& format,
    const Args&... args) noexcept {
  ::absl::Format(static_cast<::std::ostream*>(ls), format, args...);
  return *ls;
}

template <typename T,
          typename ::std::enable_if<
              LogStream::IsDirectWritable<LogStream, T>::value, int>::type>
inline LogStream& LogStream::operator<<(const T& object) noexcept {
  return write_object(object);
}

template <typename T,
          typename ::std::enable_if<
              !LogStream::IsDirectWritable<LogStream, T>::value, int>::type>
inline LogStream& LogStream::operator<<(const T& object) noexcept {
  *static_cast<::std::ostream*>(this) << object;
  return *this;
}

inline LogStream& LogStream::operator<<(
    ::std::ostream& (*function)(::std::ostream&)) noexcept {
  function(*this);
  return *this;
}

inline LogStream::LogStream(::std::streambuf& streambuf) noexcept
    : ::std::ostream(&streambuf) {}

template <typename T, typename... Args>
inline void LogStream::write_objects(const T& object,
                                     const Args&... args) noexcept {
  *this << object;
  write_objects(args...);
}

inline void LogStream::write_objects() noexcept {}

inline LogStream& LogStream::write_object(StringView sv) noexcept {
  return write(sv.data(), sv.size());
}

inline LogStream& LogStream::write_object(char c) noexcept {
  return write(c);
}

////////////////////////////////////////////////////////////////////////////////
// ScopedLogStream begin
inline ScopedLogStream::~ScopedLogStream() noexcept {
  _stream.end();
}

template <typename... Args>
inline ScopedLogStream::ScopedLogStream(LogStream& stream,
                                        Args&&... args) noexcept
    : _stream {stream} {
  _stream.begin(::std::forward<Args>(args)...);
}

inline LogStream& ScopedLogStream::stream() noexcept {
  return _stream;
}
// ScopedLogStream end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
