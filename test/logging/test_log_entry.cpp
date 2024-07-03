#include "babylon/logging/log_entry.h"
#include "babylon/reusable/page_allocator.h"

#include "gtest/gtest.h"

#include <random>

struct LogEntryTest : public ::testing::Test {
  virtual void SetUp() override {
    new_delete_page_allocator.set_page_size(128);
    page_allocator.set_upstream(new_delete_page_allocator);
    buffer.set_page_allocator(page_allocator);
  }

  virtual void TearDown() override {}

  ::std::string random_string(size_t size) {
    ::std::string s;
    s.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      s.push_back(gen());
    }
    return s;
  }

  ::std::string to_string(::std::vector<struct ::iovec>& iov) {
    ::std::string s;
    for (auto one_iov : iov) {
      s.append(static_cast<char*>(one_iov.iov_base), one_iov.iov_len);
    }
    return s;
  }

  void release(::std::vector<struct ::iovec>& iov) {
    for (auto one_iov : iov) {
      page_allocator.deallocate(one_iov.iov_base);
    }
  }

  ::std::mt19937_64 gen {::std::random_device {}()};

  ::babylon::LogStreamBuffer buffer;
  ::babylon::NewDeletePageAllocator new_delete_page_allocator;
  ::babylon::CountingPageAllocator page_allocator;
};

TEST_F(LogEntryTest, get_empty_log_entry_when_no_input) {
  buffer.begin();
  auto& entry = buffer.end();
  ASSERT_EQ(0, entry.size);
  ASSERT_EQ(0, page_allocator.allocated_page_num());

  ::std::vector<struct ::iovec> iov;
  entry.append_to_iovec(page_allocator.page_size(), iov);
  ASSERT_EQ(0, iov.size());
}

TEST_F(LogEntryTest, read_and_write_correct) {
  ::std::vector<struct ::iovec> iov;
  auto s = random_string(128 * 128);
  for (size_t i = 0; i < 128 * 128; ++i) {
    ::std::string_view sv {s.c_str(), i};
    iov.clear();
    buffer.begin();
    buffer.sputn(sv.data(), sv.size());
    auto& entry = buffer.end();
    entry.append_to_iovec(page_allocator.page_size(), iov);
    auto ss = to_string(iov);
    ASSERT_EQ(sv, ss);
    release(iov);
    ASSERT_EQ(0, page_allocator.allocated_page_num());
  }
}

TEST_F(LogEntryTest, work_with_std_ostream) {
  auto s = random_string(gen() % (128 * 128));
  ::std::ostream os {&buffer};
  buffer.begin();
  os << s;
  auto& entry = buffer.end();

  ::std::vector<struct ::iovec> iov;
  entry.append_to_iovec(page_allocator.page_size(), iov);
  auto ss = to_string(iov);
  ASSERT_EQ(s, ss);
  release(iov);
  ASSERT_EQ(0, page_allocator.allocated_page_num());
}
