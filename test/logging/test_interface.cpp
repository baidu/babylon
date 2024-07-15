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
    this->file = file;
    this->line = line;
    this->severity = severity;
    return ls;
  }

  ::std::stringstream ss;
  LogStream ls {*ss.rdbuf()};
  ::std::string file;
  int line {-1};
  int severity {-1};
};

struct LogInterfaceTest : public ::testing::Test {
  virtual void SetUp() override {
    LogInterface::set_min_severity(LogInterface::SEVERITY_INFO);
    LogInterface::set_provider(nullptr);
  }
};

TEST_F(LogInterfaceTest, ignore_log_less_than_min_severity) {
  LogInterface::set_min_severity(LogInterface::SEVERITY_WARNING);
  ::testing::internal::CaptureStderr();
  BABYLON_LOG(INFO) << "this line should not appear in stderr";
  auto text = ::testing::internal::GetCapturedStderr();
  ASSERT_TRUE(text.empty());
}

TEST_F(LogInterfaceTest, change_log_backend) {
  LogInterface::set_provider(
      ::std::unique_ptr<LogStreamProvider>(new MockLogStreamProvider));
  auto file = __FILE__;
  auto before_line = __LINE__;
  BABYLON_LOG(INFO) << "this line should appear in provider";
  auto after_line = __LINE__;
  MockLogStreamProvider& provider =
      dynamic_cast<MockLogStreamProvider&>(LogInterface::provider());
  ASSERT_NE(::std::string::npos,
            provider.ss.str().find("this line should appear in provider"));
  ASSERT_EQ(file, provider.file);
  ASSERT_LT(before_line, provider.line);
  ASSERT_GT(after_line, provider.line);
  ASSERT_TRUE(LogInterface::SEVERITY_INFO == provider.severity);
}
