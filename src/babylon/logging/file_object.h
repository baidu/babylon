#pragma once

#include "babylon/environment.h"

#include <cstdint> // SIZE_MAX
#include <tuple>   // std::tuple

BABYLON_NAMESPACE_BEGIN

// 日志文件实体，用于解耦文件滚动管理和实际的读写动作
// 实现内部封装截断/滚动/磁盘容量控制等功能
// 对上层提供提供最终准备好的文件描述符作为分界面
class AsyncFileAppender;
class FileObject {
 public:
  // 可以默认构造和移动，不可拷贝
  FileObject() noexcept = default;
  FileObject(FileObject&&) noexcept = default;
  FileObject(const FileObject&) = delete;
  FileObject& operator=(FileObject&&) noexcept = default;
  FileObject& operator=(const FileObject&) = delete;
  virtual ~FileObject() noexcept = default;

  // 核心功能函数，上层在每次写出前需要调用此函数来获得文件操作符
  // 函数内部完成文件滚动检测等操作，返回最终准备好的描述符
  // 由于可能发生文件的滚动，返回值为新旧描述符(fd, old_fd)二元组
  // fd:
  //   >=0: 当前文件描述符，调用者后续写入通过此描述符发起
  //   < 0: 发生异常无法打开文件
  // old_fd:
  //   >=0: 发生文件切换，返回之前的文件描述符
  //        一般由文件滚动引起，需要调用者执行关闭动作
  //        关闭前调用者可以进行最后的收尾写入等操作
  //   < 0: 未发生文件切换
  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept = 0;

 private:
  // 由AsyncFileAppender内部使用，用于记录一些附加信息
  void set_index(size_t index) noexcept;
  size_t index() const noexcept;

  size_t _index {SIZE_MAX};

  friend class AsyncFileAppender;
};

// 指向标准错误流文件的包装
class StderrFileObject : public FileObject {
 public:
  static StderrFileObject& instance() noexcept;

 private:
  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept
      override;
};

BABYLON_NAMESPACE_END
