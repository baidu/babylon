#include "babylon/logging/file_object.h"

#include <unistd.h> // STDERR_FILENO

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// FileObject::begin
void FileObject::set_index(size_t index) noexcept {
  _index = index;
}

size_t FileObject::index() const noexcept {
  return _index;
}
// FileObject::end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// StderrFileObject begin
StderrFileObject& StderrFileObject::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static StderrFileObject object;
#pragma GCC diagnostic pop
  return object;
}

::std::tuple<int, int>
StderrFileObject::check_and_get_file_descriptor() noexcept {
  return ::std::tuple<int, bool>(STDERR_FILENO, -1);
}
// StderrFileObject end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
