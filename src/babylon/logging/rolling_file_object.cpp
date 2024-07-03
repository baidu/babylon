#include "babylon/logging/rolling_file_object.h"

#include "re2/re2.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

BABYLON_NAMESPACE_BEGIN

RollingFileObject::~RollingFileObject() noexcept {
  if (_background_thread.joinable()) {
    _running.store(false, ::std::memory_order_release);
    _background_thread.join();
  }
}

void RollingFileObject::set_directory(StringView directory) noexcept {
  _directory.assign(directory.data(), directory.size());
}

void RollingFileObject::set_file_pattern(StringView pattern) noexcept {
  _file_pattern.assign(pattern.data(), pattern.size());
}

void RollingFileObject::set_max_file_size(size_t size) noexcept {
  _max_file_size = size;
}

void RollingFileObject::set_interval_s(size_t seconds) noexcept {
  _interval_s = seconds;
}

void RollingFileObject::set_max_file_number(size_t number) noexcept {
  _max_file_number = number;
}

void RollingFileObject::set_auto_delete(bool enable) noexcept {
  _auto_delete = enable;
}

int RollingFileObject::initialize() noexcept {
  ::std::cerr << "directory: " << _directory << ::std::endl;
  ::std::cerr << "file pattern: " << _file_pattern << ::std::endl;

  ::mkdir(_directory.c_str(), 0755);
  if (0 != scan_and_tracking_existing_files()) {
    return -1;
  }

  for (auto& file : _tracking_files) {
    ::std::cerr << "tracking: " << file << ::std::endl;
  }

  if (_auto_delete && _max_file_number != SIZE_MAX) {
    ::std::cerr << "start back" << ::std::endl;
    _running.store(true, ::std::memory_order_release);
    _background_thread =
        ::std::thread(&RollingFileObject::keep_delete_expire_file, this);
  }
  return 0;
}

void RollingFileObject::delete_expire_files() noexcept {
  ::std::cerr << "call delete" << ::std::endl;
  if (_max_file_number == SIZE_MAX) {
    return;
  }

  ::std::vector<::std::string> to_be_deleted_files;
  {
    ::std::lock_guard<::std::mutex> {_tracking_files_mutex};
    ::std::cerr << "size: " << _tracking_files.size() << ::std::endl;
    while (_tracking_files.size() > _max_file_number) {
      to_be_deleted_files.emplace_back(::std::move(_tracking_files.front()));
      _tracking_files.pop_front();
    }
  }

  for (auto& file : to_be_deleted_files) {
    ::std::cerr << "delete file: " << file << ::std::endl;
    ::unlink(file.c_str());
  }
}

int RollingFileObject::scan_and_tracking_existing_files() noexcept {
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
  ::std::cerr << "file re: " << file_re << ::std::endl;
  ::RE2 file_matcher(file_re);
  if (!file_matcher.ok()) {
    return -1;
  }

  auto dir = ::opendir(_directory.c_str());
  ::std::cerr << "opendir: " << dir << ::std::endl;
  if (dir == nullptr) {
    return -1;
  }

  for (struct ::dirent* entry = ::readdir(dir); entry != nullptr;
       entry = ::readdir(dir)) {
    ::std::cerr << "readdir: " << entry->d_name << ::std::endl;
    if (RE2::FullMatch(entry->d_name, file_matcher)) {
      ::std::cerr << "match: " << entry->d_name << ::std::endl;
      ::std::lock_guard<::std::mutex> lock {_tracking_files_mutex};
      _tracking_files.emplace_back(_directory);
      _tracking_files.back().push_back('/');
      _tracking_files.back().append(entry->d_name);
    }
  }
  ::closedir(dir);

  _tracking_files.sort();
  return 0;
}

void RollingFileObject::keep_delete_expire_file() noexcept {
  ::std::cerr << "in keep" << ::std::endl;
  while (_running.load(::std::memory_order_acquire)) {
    ::std::cerr << "delete once" << ::std::endl;
    delete_expire_files();
    ::usleep(1000 * 1000);
  }
}

::std::tuple<int, int>
RollingFileObject::check_and_get_file_descriptor() noexcept {
  if (ABSL_PREDICT_FALSE(0 != check())) {
    open();
  }
  return ::std::make_tuple(_fd, -1);
}

int RollingFileObject::check() noexcept {
  // 文件未打开
  if (ABSL_PREDICT_FALSE(_fd < 0)) {
    return -1;
  }
  // 最频繁一秒检查一次足以
  // auto now_s = ::base::gettimeofday_s();
  // if (now_s <= _last_check_time_s) {
  //    return 0;
  //}
  //_last_check_time_s = now_s;

  // if (0 != check_size()) {
  //     return -1;
  // }
  // if (0 != check_time(now_s)) {
  //     return -1;
  // }
  return 0;
}

int RollingFileObject::open() noexcept {
  /*
  ::mkdir(_directory.c_str(), 0755);
         size_t strftime(char *s, size_t max, const char *format,
                     const struct tm *tm);
  _fd = ::open(_file_name.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0666);
  if (_fd < 0) {
      return;
  }
  if (0 != ::fstat(_fd, &_fstat)) {
      ::close(_fd);
      _fd = -1;
  } else {
      // 打开成功时记录下一些时间信息
      _last_check_time_s = ::base::gettimeofday_s();
      _crono_file_time_s = ::comspace::xroundtime(_appender->_device,
  _last_check_time_s); _crono_cut_time_s = _appender->_device.splite_type ==
  COM_DATECUT ?
          (_crono_file_time_s + _appender->_device.cuttime * 60) : INT64_MAX;
  }
  */
  return 0;
}
BABYLON_NAMESPACE_END
