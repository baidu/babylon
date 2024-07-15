#pragma once

#include "babylon/string_view.h" // StringView

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
  inline constexpr LogSeverity& operator=(LogSeverity&&) noexcept = default;
  inline constexpr LogSeverity& operator=(const LogSeverity&) noexcept =
      default;
  inline ~LogSeverity() noexcept = default;

  inline constexpr LogSeverity(int8_t value) noexcept;

  inline constexpr operator int8_t() const noexcept;

  inline constexpr operator StringView() const noexcept;

 private:
  int8_t _value {DEBUG};

  static constexpr StringView names[NUM] = {
      [DEBUG] = "DEBUG",
      [INFO] = "INFO",
      [WARNING] = "WARNING",
      [FATAL] = "FATAL",
  };
};

inline constexpr LogSeverity::LogSeverity(int8_t value) noexcept
    : _value {value} {}

inline constexpr LogSeverity::operator int8_t() const noexcept {
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
