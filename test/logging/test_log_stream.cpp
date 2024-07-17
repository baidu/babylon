#include <babylon/logging/log_stream.h>

#include <gtest/gtest.h>

using ::babylon::LogStream;

struct StringLogStream : public LogStream {
  StringLogStream() : LogStream(stringbuf) {}
  virtual void do_begin() noexcept override {
    begin_times++;
  }
  virtual void do_end() noexcept override {
    end_times++;
  }

  ::std::stringbuf stringbuf;
  size_t begin_times {0};
  size_t end_times {0};
};

struct Ostreamable {
  ::std::string s;
};

static ::std::ostream& operator<<(::std::ostream& os, Ostreamable osb) {
  return os << osb.s;
}

struct Lstreamable {
  ::std::string s;
};

static LogStream& operator<<(LogStream& ls, Lstreamable lsb) {
  return ls.write(lsb.s.c_str(), lsb.s.size());
}

struct LogStreamTest : public ::testing::Test {
  virtual void SetUp() override {}

  virtual void TearDown() override {}

  StringLogStream ss;
};

TEST_F(LogStreamTest, can_write_raw_bytes) {
  ss.begin();
  ss.write("hello", 3);
  ss.end();
  ASSERT_EQ("hel", ss.stringbuf.str());
}

TEST_F(LogStreamTest, can_format_as_printf) {
  ss.begin();
  ss.format("hello %.2f %g", 0.1234, 0.5678).format(" %d", 90);
  ss.end();
  ASSERT_EQ("hello 0.12 0.5678 90", ss.stringbuf.str());
}

TEST_F(LogStreamTest, can_write_as_stream) {
  ss.begin();
  ss << "hello +" << 10086 << ' ' << 0.66 << ::std::endl;
  ss.end();
  ASSERT_EQ("hello +10086 0.66\n", ss.stringbuf.str());
}

TEST_F(LogStreamTest, support_custom_stream_to_ostream) {
  ss.begin();
  ss << Ostreamable {"hello"};
  ss.end();
  ASSERT_EQ("hello", ss.stringbuf.str());
}

TEST_F(LogStreamTest, support_custom_stream_to_log_stream) {
  ss.begin();
  ss << Lstreamable {"hello"};
  ss.end();
  ASSERT_EQ("hello", ss.stringbuf.str());
}

TEST_F(LogStreamTest, call_begin_end_hook) {
  ASSERT_EQ(0, ss.begin_times);
  ss.begin();
  ASSERT_EQ(1, ss.begin_times);
  ss << "hello";
  ASSERT_EQ(0, ss.end_times);
  ss.end();
  ASSERT_EQ(1, ss.end_times);

  ASSERT_EQ(1, ss.begin_times);
  ss.begin();
  ASSERT_EQ(2, ss.begin_times);
  ss << "hello";
  ASSERT_EQ(1, ss.end_times);
  ss.end();
  ASSERT_EQ(2, ss.end_times);
}

TEST_F(LogStreamTest, begin_with_header) {
  ss.begin("hello +", 10086, " ");
  ss << "world";
  ss.end();
  ASSERT_EQ("hello +10086 world", ss.stringbuf.str());
}

TEST_F(LogStreamTest, noflush_join_two_transaction) {
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  (ss << "world").noflush();
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << " " << 0.66;
  ss.end();
  ASSERT_EQ(1, ss.end_times);
  ASSERT_EQ("hello +10086 world 0.66", ss.stringbuf.str());
}

TEST_F(LogStreamTest, noflush_effective_only_once) {
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  (ss << "world").noflush();
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << " " << 0.66;
  ss.end();
  ASSERT_EQ(1, ss.end_times);
  ss.begin(" ");
  ASSERT_EQ(2, ss.begin_times);
  ss << 10010;
  ss.end();
  ASSERT_EQ(2, ss.end_times);
  ASSERT_EQ("hello +10086 world 0.66 10010", ss.stringbuf.str());
}

TEST_F(LogStreamTest, reenterable_as_noflush) {
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << "world";
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << " ";
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  ss << 0.66;
  ss.end();
  ASSERT_EQ(1, ss.end_times);
  ASSERT_EQ("hello +10086 world 0.66", ss.stringbuf.str());
}

TEST_F(LogStreamTest, ignore_noflush_on_reenter) {
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  (ss << "world").noflush();
  ss << " ";
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  ss << 0.66;
  ss.end();
  ASSERT_EQ(1, ss.end_times);
  ASSERT_EQ("hello +10086 world 0.66", ss.stringbuf.str());
}

TEST_F(LogStreamTest, noflushable_before_reenter) {
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  (ss << "world").noflush();
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << " ";
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  ss << 0.66;
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << " " << 10010;
  ss.end();
  ASSERT_EQ(1, ss.end_times);
  ASSERT_EQ("hello +10086 world 0.66 10010", ss.stringbuf.str());
}

TEST_F(LogStreamTest, noflushable_after_reenter) {
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << "world";
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << " ";
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  (ss << 0.66).noflush();
  ss.end();
  ASSERT_EQ(0, ss.end_times);
  ss.begin("hello +", 10086, " ");
  ASSERT_EQ(1, ss.begin_times);
  ss << " " << 10010;
  ss.end();
  ASSERT_EQ(1, ss.end_times);
  ASSERT_EQ("hello +10086 world 0.66 10010", ss.stringbuf.str());
}

TEST_F(LogStreamTest, default_log_stream_log_to_stderr) {
  ::babylon::DefaultLogStream dls;
  ::testing::internal::CaptureStderr();
  dls.begin();
  dls << "this should apper in stderr";
  dls.end();
  auto text = ::testing::internal::GetCapturedStderr();
  ASSERT_NE(text.npos, text.find("this should apper in stderr"));
}
