#include <babylon/logging/async_file_appender.h>

#include <fcntl.h>
#include <gtest/gtest.h>

#include <random>

using ::babylon::AsyncFileAppender;
using ::babylon::FileObject;
using ::babylon::Log;
using ::babylon::LogStreamBuffer;
using ::babylon::PageAllocator;
using ::babylon::PageHeap;

struct StaticFileObject : public FileObject {
  virtual int check_and_get_file_descriptor() noexcept override {
    return fd;
  }
  int fd;
};

struct LogStream : public ::std::ostream {
  LogStream(PageAllocator& page_allocator) noexcept : ::std::ostream(&buffer) {
    buffer.begin(page_allocator);
  }

  inline Log& end() noexcept {
    return buffer.end();
  }

  LogStreamBuffer buffer;
};

struct AsyncFileAppenderTest : public ::testing::Test {
  virtual void SetUp() override {
    ASSERT_EQ(0, ::pipe(pipefd));
    int ret = ::fcntl(pipefd[1], F_GETPIPE_SZ);
    fprintf(stderr, "pipe buffer size %d\n", ret);
    ret = ::fcntl(pipefd[1], F_SETPIPE_SZ, 16384);
    fprintf(stderr, "set pipe buffer size ret %d\n", ret);
    file_object.fd = pipefd[1];
  }

  virtual void TearDown() override {
    close(pipefd[1]);
    close(pipefd[0]);
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
  PageHeap page_heap;
  AsyncFileAppender appender;
};

TEST_F(AsyncFileAppenderTest, default_write_to_stderr) {
  ASSERT_EQ(0, appender.initialize());
  LogStream ls(appender.page_allocator());
  ls << "this line should appear in stderr with num " << 10086 << ::std::endl;
  appender.write(ls.end());
}

TEST_F(AsyncFileAppenderTest, write_to_file_object) {
  appender.set_file_object(file_object);
  ASSERT_EQ(0, appender.initialize());
  LogStream ls(appender.page_allocator());
  ls << "this line should appear in pipe with num " << 10010 << ::std::endl;
  appender.write(ls.end());

  ::std::string expected = "this line should appear in pipe with num 10010\n";
  ::std::string s;
  s.resize(expected.size());
  read_pipe(&s[0], s.size());
  ASSERT_EQ(expected, s);
}

TEST_F(AsyncFileAppenderTest, write_happen_async) {
  appender.set_file_object(file_object);
  ASSERT_EQ(0, appender.initialize());
  for (size_t i = 0; i < 1000; ++i) {
    LogStream ls(appender.page_allocator());
    ls << "this line should appear in pipe with num " << i << ::std::endl;
    appender.write(ls.end());
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
}

TEST_F(AsyncFileAppenderTest, can_discard_log) {
  appender.set_file_object(file_object);
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

  page_heap.set_page_size(page_size);
  appender.set_page_allocator(page_heap);
  appender.set_file_object(file_object);
  appender.set_queue_capacity(64);
  ASSERT_EQ(0, appender.initialize());

  for (size_t i = page_size / 2; i < max_log_size; i += page_size / 2) {
    LogStream ls(appender.page_allocator());
    ls << s.substr(0, i);
    appender.write(ls.end());

    ::std::string rs;
    rs.resize(i);
    read_pipe(&rs[0], rs.size());
    ASSERT_EQ(s.substr(0, i), rs);
  }
}
