#pragma once

#include "babylon/string_view.h"

BABYLON_IO_NAMESPACE_BEGIN

class Error {
 public:
  Error() noexcept = default;

  explicit Error(int code) noexcept;

  int code() const noexcept;
  StringView text() const noexcept;

 private:
  int _errno {errno};
};

template <typename C, typename T>
inline ::std::basic_ostream<C, T>& operator<<(::std::basic_ostream<C, T>& os,
                                              const Error& error) {
  return os << "Error[" << error.code() << ":" << error.text() << "]";
}

BABYLON_IO_NAMESPACE_END
