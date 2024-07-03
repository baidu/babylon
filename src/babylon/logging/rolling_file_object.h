#pragma once

#include "babylon/logging/async_file_appender.h" // babylon::FileObject

#include <sys/stat.h> // ::stat

#include <list>  // std::list
#include <mutex> // std::mutex

BABYLON_NAMESPACE_BEGIN

class RollingFileObject : public FileObject {
 public:
  virtual ~RollingFileObject() noexcept override;

  void set_directory(StringView directory) noexcept;
  void set_file_pattern(StringView pattern) noexcept;
  void set_max_file_size(size_t size) noexcept;
  void set_interval_s(size_t seconds) noexcept;
  void set_max_file_number(size_t number) noexcept;
  void set_auto_delete(bool enable) noexcept;

  int initialize() noexcept;

  void delete_expire_files() noexcept;

 private:
  int scan_and_tracking_existing_files() noexcept;

  void keep_delete_expire_file() noexcept;

  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept
      override;

  int check() noexcept;
  int open() noexcept;

  ::std::string _directory;
  ::std::string _file_pattern;
  size_t _max_file_size {SIZE_MAX};
  size_t _interval_s {SIZE_MAX};
  size_t _max_file_number {SIZE_MAX};
  bool _auto_delete {true};

  ::std::atomic<bool> _running {false};
  ::std::thread _background_thread;

  int _fd {-1};
  struct ::stat _stat;

  ::std::mutex _tracking_files_mutex;
  ::std::list<::std::string> _tracking_files;
};

BABYLON_NAMESPACE_END
