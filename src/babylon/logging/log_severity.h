#pragma once

#include "babylon/string_view.h" // StringView

// clang-format off
#include "babylon/protect.h"
// clang-format on

#include <cstdint> // uint8_t

BABYLON_NAMESPACE_BEGIN

class LogSeverity {
 public:
  enum E {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    FATAL = 3,

    NUM = 4,
  };

  inline constexpr LogSeverity() noexcept = default;
  inline constexpr LogSeverity(LogSeverity&&) noexcept = default;
  inline constexpr LogSeverity(const LogSeverity&) noexcept = default;
  inline CONSTEXPR_SINCE_CXX14 LogSeverity& operator=(LogSeverity&&) noexcept =
      default;
  inline CONSTEXPR_SINCE_CXX14 LogSeverity& operator=(
      const LogSeverity&) noexcept = default;
  inline ~LogSeverity() noexcept = default;

  inline constexpr LogSeverity(uint8_t value) noexcept;

  inline constexpr operator uint8_t() const noexcept;

  inline constexpr operator StringView() const noexcept;

 private:
  uint8_t _value {DEBUG};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
  static constexpr StringView names[NUM] = {
      [DEBUG] = "DEBUG",
      [INFO] = "INFO",
      [WARNING] = "WARNING",
      [FATAL] = "FATAL",
  };
#pragma GCC diagnostic pop
};

inline constexpr LogSeverity::LogSeverity(uint8_t value) noexcept
    : _value {value} {}

inline constexpr LogSeverity::operator uint8_t() const noexcept {
  return _value;
}

inline constexpr LogSeverity::operator StringView() const noexcept {
  return names[_value];
}

template <typename C, typename T>
inline ::std::basic_ostream<C, T>& operator<<(::std::basic_ostream<C, T>& os,
                                              LogSeverity severity) noexcept {
  return os << static_cast<StringView>(severity);
}

BABYLON_NAMESPACE_END

#include "babylon/unprotect.h"
