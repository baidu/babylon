#pragma once

#include "babylon/logging/file_object.h" // FileObject
#include "babylon/string_view.h"         // StringView

#if __cplusplus >= 201703L

#include <sys/stat.h> // ::stat

#include <atomic> // std::atomic
#include <list>   // std::list
#include <mutex>  // std::mutex
#include <thread> // std::thread

BABYLON_NAMESPACE_BEGIN

// 定时滚动的日志文件实体
// - 文件名通过pattern设定，根据strftime语法根据时间生成
// - 根据文件名变化切换打开滚动到新文件
// - 跟踪滚动文件总数，执行定量清理
class RollingFileObject : public FileObject {
 public:
  virtual ~RollingFileObject() noexcept override;

  // 设置文件所属目录
  void set_directory(StringView directory) noexcept;
  // 设置文件pattern，接受strftime语法，例如典型的%Y%m%d%H
  void set_file_pattern(StringView pattern) noexcept;
  // 设置最多保留文件数，默认无限保留
  void set_max_file_number(size_t number) noexcept;

  // 在限定了保留文件数的情况下，会对创建的文件加入跟踪列表用于定量保留
  // 但是对于已经存在的文件无法跟踪
  // 启动期间调用此接口可以扫描目录并记录其中符合pattern的已有文件
  // 并加入到跟踪列表，来支持重启场景下继续跟进正确的文件定量保留
  void scan_and_tracking_existing_files() noexcept;
  // 检查目前跟踪列表中是否超出了保留数目，超出则进行清理
  void delete_expire_files() noexcept;

 private:
  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept
      override;

  int format_file_name(char* buffer, size_t size) noexcept;
  int open() noexcept;

  ::std::string _directory;
  ::std::string _file_pattern;
  size_t _max_file_number {SIZE_MAX};

  time_t _last_check_time {0};

  int _fd {-1};
  ::std::string _file_name;
  struct ::stat _stat;

  ::std::mutex _tracking_files_mutex;
  ::std::list<::std::string> _tracking_files;
};

BABYLON_NAMESPACE_END

#endif // __cplusplus >= 201703L
