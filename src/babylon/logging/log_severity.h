#pragma once

#include "babylon/string_view.h" // StringView

BABYLON_NAMESPACE_BEGIN

enum class LogSeverity : int8_t {
  DEBUG = 0,
  INFO = 1,
  WARNING = 2,
  FATAL = 3,

  NUM = 4,
};

template <typename C, typename T>
inline ::std::basic_ostream<C, T>& operator<<(::std::basic_ostream<C, T>& os,
                                              LogSeverity severity) noexcept {
  static constexpr StringView names[] = {
      "DEBUG",
      "INFO",
      "WARNING",
      "FATAL",
  };
  return os << names[static_cast<int>(severity)];
}

BABYLON_NAMESPACE_END
