#pragma once

#include "babylon/logging/log_stream.h" // babylon::LogStream

BABYLON_NAMESPACE_BEGIN

class AsyncLogStream : public LogStream {
 public:
  AsyncLogStream() noexcept;

 private:
  virtual void do_begin() noexcept override;
  virtual void do_end() noexcept override;

  ::std::mutex _mutex;
};

BABYLON_NAMESPACE_END

#include "babylon/logging/log_stream.hpp"
