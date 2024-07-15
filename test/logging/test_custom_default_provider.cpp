#include "babylon/logging/interface.h"
#include "babylon/logging/logger.h"

#include <gtest/gtest.h>

using ::babylon::LogInterface;
using ::babylon::LogStream;
using ::babylon::LogStreamProvider;
using ::babylon::StringView;

struct MockLogStreamProvider : public LogStreamProvider {
  virtual LogStream& stream(int severity, StringView file,
                            int line) noexcept override {
    this->severity = severity;
    this->file = file;
    this->line = line;
    return ls;
  }

  ::std::stringstream ss;
  LogStream ls {*ss.rdbuf()};
  StringView file;
  int line {-1};
  int severity {-1};
};

static MockLogStreamProvider provider;

BABYLON_NAMESPACE_BEGIN
LogStreamProvider* LogInterface::_s_provider {&::provider};
BABYLON_NAMESPACE_END

TEST(LogInterfaceTest, default_log_to_custom__backend) {
  auto file = __FILE__;
  auto before_line = __LINE__;
  BABYLON_LOG(INFO) << "this line should appear in provider";
  auto after_line = __LINE__;
  ASSERT_NE(::std::string::npos,
            provider.ss.str().find("this line should appear in provider"));
  ASSERT_EQ(file, provider.file);
  ASSERT_LT(before_line, provider.line);
  ASSERT_GT(after_line, provider.line);
  ASSERT_TRUE(LogInterface::SEVERITY_INFO == provider.severity);
}
