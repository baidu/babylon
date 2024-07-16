#include "babylon/logging/async_file_appender.h"
#include "babylon/logging/async_log_stream.h"

#include "gtest/gtest.h"

#include <random>

using ::babylon::AsyncFileAppender;
using ::babylon::AsyncLogStream;
using ::babylon::FileObject;
using ::babylon::LoggerBuilder;

namespace {
struct StaticFileObject : public FileObject {
  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept
      override {
    return ::std::tuple<int, int> {fd, -1};
  }
  int fd;
};
} // namespace

struct AsyncLogStreamTest : public ::testing::Test {
  virtual void SetUp() override {
    file_object.fd = STDERR_FILENO;
    ASSERT_EQ(0, appender.initialize());
  }

  StaticFileObject file_object;
  AsyncFileAppender appender;
};

TEST_F(AsyncLogStreamTest, write_to_file_object) {
  LoggerBuilder builder;
  builder.set_log_stream_creator(
      AsyncLogStream::creator(appender, file_object));
  ::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
  ::babylon::LoggerManager::instance().apply();

  ::testing::internal::CaptureStderr();
  BABYLON_LOG(INFO) << "this line should appear in stderr with num " << 10010;
  appender.close();
  auto text = ::testing::internal::GetCapturedStderr();

  ::std::cerr << text;
  ASSERT_NE(text.npos,
            text.find("this line should appear in stderr with num 10010"));
}

TEST_F(AsyncLogStreamTest, write_header_before_message) {
  LoggerBuilder builder;
  builder.set_log_stream_creator(
      AsyncLogStream::creator(appender, file_object, [](AsyncLogStream& ls) {
        ls << "[this header appear before message]";
      }));
  ::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
  ::babylon::LoggerManager::instance().apply();

  ::testing::internal::CaptureStderr();
  BABYLON_LOG(INFO) << "this line should appear in stderr with num"
                    << ::babylon::noflush;
  BABYLON_LOG(INFO) << " " << 10010;
  appender.close();
  auto text = ::testing::internal::GetCapturedStderr();

  ::std::cerr << text;
  ASSERT_NE(text.npos,
            text.find("this line should appear in stderr with num 10010"));
  text.resize(text.find("this line should appear in stderr with num 10010"));
  ASSERT_EQ("[this header appear before message]", text);
}
