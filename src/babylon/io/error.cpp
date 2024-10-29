#define _POSIX_C_SOURCE 200809L
#undef _GNU_SOURCE

#include "babylon/io/error.h"

#include <string.h>

BABYLON_IO_NAMESPACE_BEGIN

Error::Error(int code) noexcept : _errno {code} {}

int Error::code() const noexcept {
  return _errno;
}

StringView Error::text() const noexcept {
  static thread_local char buffer[256];
#if _POSIX_C_SOURCE >= 200112L && !_GNU_SOURCE
  auto ret = ::strerror_r(_errno, buffer, sizeof(buffer));
  return ret == 0 ? buffer : "";
#else  // _POSIX_C_SOURCE < 200112L || _GNU_SOURCE
  errno = 0;
  auto result = ::strerror_r(_errno, buffer, sizeof(buffer));
  return errno == 0 ? result : "";
#endif // _POSIX_C_SOURCE < 200112L || _GNU_SOURCE
}

BABYLON_IO_NAMESPACE_END
