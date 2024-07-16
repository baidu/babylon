#include <babylon/logging/async_file_appender.h>

#include <fcntl.h>
#include <gtest/gtest.h>

#include <random>

using ::babylon::AsyncFileAppender;
using ::babylon::FileObject;
using ::babylon::LogEntry;
using ::babylon::LogStreamBuffer;
using ::babylon::PageAllocator;

namespace {
struct StaticFileObject : public FileObject {
  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept
      override {
    return ::std::tuple<int, int> {fd, -1};
  }
  int fd;
};
} // namespace

struct LogStream : public ::std::ostream {
  LogStream(PageAllocator& page_allocator) noexcept : ::std::ostream(&buffer) {
    buffer.set_page_allocator(page_allocator);
    buffer.begin();
  }

  inline LogEntry& end() noexcept {
    return buffer.end();
  }

  LogStreamBuffer buffer;
};

struct AsyncFileAppenderTest : public ::testing::Test {
  virtual void SetUp() override {
    ASSERT_EQ(0, ::pipe(pipefd));
    ASSERT_GT(65536, ::fcntl(pipefd[1], F_SETPIPE_SZ, 16384));
    file_object.fd = pipefd[1];
  }

  virtual void TearDown() override {
    ASSERT_EQ(0, close(pipefd[1]));
    ASSERT_EQ(0, close(pipefd[0]));
  }

  void read_pipe(char* data, size_t size) {
    while (size > 0) {
      auto got = ::read(pipefd[0], data, size);
      data += got;
      size -= got;
    }
  }

  int pipefd[2];
  StaticFileObject file_object;
  AsyncFileAppender appender;
};

TEST_F(AsyncFileAppenderTest, write_to_file_object) {
  ASSERT_EQ(0, appender.initialize());
  LogStream ls(appender.page_allocator());
  ls << "this line should appear in pipe with num " << 10010 << ::std::endl;
  appender.write(ls.end(), &file_object);

  ::std::string expected = "this line should appear in pipe with num 10010\n";
  ::std::string s;
  s.resize(expected.size());
  read_pipe(&s[0], s.size());
  ASSERT_EQ(expected, s);
}

TEST_F(AsyncFileAppenderTest, write_happen_async) {
  ASSERT_EQ(0, appender.initialize());
  for (size_t i = 0; i < 1000; ++i) {
    LogStream ls(appender.page_allocator());
    ls << "this line should appear in pipe with num " << i << ::std::endl;
    appender.write(ls.end(), &file_object);
  }
  ASSERT_LT(0, appender.pending_size());
  for (size_t i = 0; i < 1000; ++i) {
    ::std::string expected = "this line should appear in pipe with num " +
                             ::std::to_string(i) + "\n";
    ::std::string s;
    s.resize(expected.size());
    read_pipe(&s[0], s.size());
    ASSERT_EQ(expected, s);
  }
  ASSERT_EQ(0, appender.pending_size());
  appender.close();
}

TEST_F(AsyncFileAppenderTest, can_discard_log) {
  ASSERT_EQ(0, appender.initialize());
  for (size_t i = 0; i < 100; ++i) {
    LogStream ls(appender.page_allocator());
    ls << "this line should be discarded" << ::std::endl;
    appender.discard(ls.end());
  }
  ASSERT_EQ(0, appender.pending_size());
}

TEST_F(AsyncFileAppenderTest, write_different_size_level_correct) {
  size_t page_size = 512;
  size_t max_log_size = page_size / 8 * 3 * page_size;
  ::std::mt19937 gen(::std::random_device {}());
  ::std::string s;
  s.reserve(max_log_size);
  for (size_t i = 0; i < max_log_size; i++) {
    s.push_back(gen());
  }

  ::babylon::NewDeletePageAllocator page_allocator;
  page_allocator.set_page_size(page_size);
  appender.set_page_allocator(page_allocator);
  appender.set_queue_capacity(64);
  ASSERT_EQ(0, appender.initialize());

  for (size_t i = page_size / 2; i < max_log_size; i += page_size / 2) {
    LogStream ls(appender.page_allocator());
    ls << s.substr(0, i);
    appender.write(ls.end(), &file_object);

    ::std::string rs;
    rs.resize(i);
    read_pipe(&rs[0], rs.size());
    ASSERT_EQ(s.substr(0, i), rs);
  }
  appender.close();
}
