#include "babylon/logging/logger.h"

#include <gtest/gtest.h>

using ::babylon::LogInterface;
using ::babylon::LogStream;
using ::babylon::LogStreamProvider;
using ::babylon::StringView;

static ::std::stringbuf buffer;

BABYLON_NAMESPACE_BEGIN
void DefaultLoggerManagerInitializer::initialize(
    LoggerManager& manager) noexcept {
  LoggerBuilder builder;
  builder.set_log_stream_creator([] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  manager.set_root_builder(::std::move(builder));
  manager.apply();
}
BABYLON_NAMESPACE_END

TEST(DefaultLoggerManagerInitializer, custom_logger_manager_statically) {
  BABYLON_LOG(INFO) << "this line should appear in provider";
  ASSERT_NE(::std::string::npos,
            buffer.str().find("this line should appear in provider"));
}
