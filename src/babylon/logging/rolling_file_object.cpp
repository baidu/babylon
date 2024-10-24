#include "babylon/logging/rolling_file_object.h"

#if __cplusplus >= 201703L

#include "babylon/regex.h"
#include "babylon/time.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/optimization.h) // ABSL_PREDICT_FALSE
#include BABYLON_EXTERNAL(absl/time/clock.h) // absl::Now
// clang-format on

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

BABYLON_NAMESPACE_BEGIN

RollingFileObject::~RollingFileObject() noexcept {
  if (_fd >= 0) {
    ::close(_fd);
  }
}

void RollingFileObject::set_directory(StringView directory) noexcept {
  _directory.assign(directory.data(), directory.size());
}

void RollingFileObject::set_file_pattern(StringView pattern) noexcept {
  _file_pattern.assign(pattern.data(), pattern.size());
}

void RollingFileObject::set_max_file_number(size_t number) noexcept {
  _max_file_number = number;
}

void RollingFileObject::delete_expire_files() noexcept {
  if (_max_file_number == SIZE_MAX) {
    return;
  }

  ::std::vector<::std::string> to_be_deleted_files;
  {
    ::std::lock_guard<::std::mutex> lock {_tracking_files_mutex};
    while (_tracking_files.size() > _max_file_number) {
      to_be_deleted_files.emplace_back(::std::move(_tracking_files.front()));
      _tracking_files.pop_front();
    }
  }

  for (auto& file : to_be_deleted_files) {
    ::unlink(file.c_str());
  }
}

void RollingFileObject::scan_and_tracking_existing_files() noexcept {
  if (_max_file_number == SIZE_MAX) {
    return;
  }

  ::std::string file_re;
  file_re.reserve(_file_pattern.size() * 2);
  bool in_escape = false;
  bool in_repeat = false;
  for (auto c : _file_pattern) {
    if (in_escape) {
      if (c == '%') {
        file_re.push_back(c);
        in_repeat = false;
      } else if (!in_repeat) {
        file_re.append(".+");
        in_repeat = true;
      }
      in_escape = false;
      continue;
    } else if (c == '%') {
      in_escape = true;
      continue;
    } else if (c == '.') {
      file_re.push_back('\\');
    }
    file_re.push_back(c);
    in_repeat = false;
  }
  ::std::regex file_matcher(file_re);

  auto dir = ::opendir(_directory.c_str());
  if (dir == nullptr) {
    return;
  }

  for (struct ::dirent* entry = ::readdir(dir); entry != nullptr;
       entry = ::readdir(dir)) {
    if (::std::regex_match(entry->d_name, file_matcher)) {
      ::std::lock_guard<::std::mutex> lock {_tracking_files_mutex};
      _tracking_files.emplace_back(_directory);
      _tracking_files.back().push_back('/');
      _tracking_files.back().append(entry->d_name);
    }
  }
  ::closedir(dir);

  _tracking_files.sort();
  return;
}

::std::tuple<int, int>
RollingFileObject::check_and_get_file_descriptor() noexcept {
  // 文件未打开
  if (ABSL_PREDICT_FALSE(_fd < 0)) {
    open();
    return ::std::make_tuple(_fd, -1);
  }

  // 最频繁一秒检查一次足以
  auto now_time = ::absl::GetCurrentTimeNanos() / 1000 / 1000 / 1000;
  if (now_time == _last_check_time) {
    return ::std::make_tuple(_fd, -1);
  }
  _last_check_time = now_time;

  // 文件名未发生变更，继续使用
  char file_name[_file_pattern.size() + 64];
  format_file_name(file_name, sizeof(file_name));
  if (_file_name == file_name) {
    return ::std::make_tuple(_fd, -1);
  }

  // 文件切换
  auto old_fd = open();
  return ::std::make_tuple(_fd, old_fd);
}

int RollingFileObject::format_file_name(char* buffer, size_t size) noexcept {
  auto now_time = ::absl::ToTimeT(::absl::Now());
  struct ::tm time_struct;
  ::babylon::localtime(&now_time, &time_struct);
  return ::strftime(buffer, size, _file_pattern.c_str(), &time_struct);
}

int RollingFileObject::open() noexcept {
  ::mkdir(_directory.c_str(), 0755);

  char full_file_name[_directory.size() + _file_pattern.size() + 64];
  __builtin_memcpy(full_file_name, _directory.c_str(), _directory.size());
  full_file_name[_directory.size()] = '/';
  auto bytes = format_file_name(full_file_name + _directory.size() + 1,
                                _file_pattern.size() + 63);
  if (bytes == 0) {
    return -1;
  }

  auto new_fd = ::open(full_file_name, O_CREAT | O_APPEND | O_WRONLY, 0644);
  if (new_fd < 0) {
    return -1;
  }

  if (0 != ::fstat(new_fd, &_stat)) {
    ::close(new_fd);
    return -1;
  }

  if (_max_file_number != SIZE_MAX) {
    ::std::lock_guard<::std::mutex> lock {_tracking_files_mutex};
    _tracking_files.push_back(full_file_name);
  }
  auto old_fd = _fd;
  _fd = new_fd;
  _file_name = full_file_name + _directory.size() + 1;
  return old_fd;
}

BABYLON_NAMESPACE_END

#endif // __cplusplus >= 201703L
