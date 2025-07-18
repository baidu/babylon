#include "babylon/logging/logger.h"
#include "babylon/mlock.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/config.h)
// clang-format on

#include "gtest/gtest.h"

#include <fcntl.h>
#include <sys/mman.h>

#include <fstream>

#if !BABYLON_HAS_ADDRESS_SANITIZER and !ABSL_HAVE_THREAD_SANITIZER

using ::babylon::MemoryLocker;
using ::babylon::StringView;

const static int PAGE_SIZE = sysconf(_SC_PAGESIZE);
const static size_t REGION_SIZE = 30000;
const static size_t CEILED_REGION_SIZE =
    (REGION_SIZE + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

struct MemoryLockerTest : public ::testing::Test {
  virtual void SetUp() override {
    file_name = "mlock_" + ::std::to_string(getpid());
    fd = ::open(file_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    ASSERT_EQ(0, ::ftruncate(fd, REGION_SIZE));
    region = ::mmap(nullptr, REGION_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    MemoryLocker::instance().set_check_interval(::std::chrono::seconds(1));
    MemoryLocker::instance().set_filter([this](StringView path) {
      return nullptr == strstr(path.data(), file_name.c_str());
    });
  }

  virtual void TearDown() override {
    ::munmap(region, REGION_SIZE);
    ::close(fd);
    ::unlink(file_name.c_str());
  }

  bool region_in_memory() {
    ::std::vector<uint8_t> vec(CEILED_REGION_SIZE / PAGE_SIZE);
    ::mincore(region, REGION_SIZE, vec.data());
    for (auto in : vec) {
      if (!in) {
        return false;
      }
    }
    return true;
  }

  void next_round() {
    auto current = MemoryLocker::instance().round();
    while (MemoryLocker::instance().round() == current) {
      usleep(1000);
    }
  }

  ::std::string file_name;
  MemoryLocker& locker = MemoryLocker::instance();
  int fd;
  void* region;
};

TEST_F(MemoryLockerTest, lock_regions_before_start) {
  ASSERT_FALSE(region_in_memory());
  ASSERT_EQ(0, MemoryLocker::instance().start());
  next_round();
  BABYLON_LOG(INFO) << "before check region_in_memory";
  ASSERT_TRUE(region_in_memory());
  ASSERT_EQ(CEILED_REGION_SIZE, MemoryLocker::instance().locked_bytes());
  ASSERT_EQ(0, MemoryLocker::instance().last_errno());
  ASSERT_EQ(0, MemoryLocker::instance().stop());
}

TEST_F(MemoryLockerTest, lock_regions_after_start) {
  ::munmap(region, REGION_SIZE);
  ASSERT_EQ(0, MemoryLocker::instance().start());
  next_round();
  region = ::mmap(nullptr, REGION_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
  ASSERT_FALSE(region_in_memory());
  next_round();
  ASSERT_TRUE(region_in_memory());
  ASSERT_EQ(CEILED_REGION_SIZE, MemoryLocker::instance().locked_bytes());
  ASSERT_EQ(0, MemoryLocker::instance().last_errno());
  ASSERT_EQ(0, MemoryLocker::instance().stop());
}

#if __x86_64__
TEST_F(MemoryLockerTest, unlock_regions_after_stop) {
  ASSERT_EQ(0, MemoryLocker::instance().start());
  next_round();
  ::madvise(region, REGION_SIZE, MADV_DONTNEED);
  ::posix_fadvise(fd, 0, REGION_SIZE, POSIX_FADV_DONTNEED);
  ASSERT_TRUE(region_in_memory());
  ASSERT_EQ(0, MemoryLocker::instance().stop());
  ASSERT_TRUE(region_in_memory());
  ::madvise(region, REGION_SIZE, MADV_DONTNEED);
  ::posix_fadvise(fd, 0, REGION_SIZE, POSIX_FADV_DONTNEED);
  ASSERT_FALSE(region_in_memory());
}
#endif // __x86_64__

TEST_F(MemoryLockerTest, start_twice_fail_but_harmless) {
  ASSERT_FALSE(region_in_memory());
  ASSERT_EQ(0, MemoryLocker::instance().start());
  ASSERT_NE(0, MemoryLocker::instance().start());
  next_round();
  ASSERT_TRUE(region_in_memory());
  ASSERT_EQ(CEILED_REGION_SIZE, MemoryLocker::instance().locked_bytes());
  ASSERT_EQ(0, MemoryLocker::instance().last_errno());
  ASSERT_EQ(0, MemoryLocker::instance().stop());
}

TEST_F(MemoryLockerTest, default_usable) {
  MemoryLocker locker;
  locker.set_check_interval(::std::chrono::seconds(1));
  ASSERT_EQ(0, locker.start());
  ASSERT_EQ(0, locker.stop());
}

TEST_F(MemoryLockerTest, stop_when_destroy) {
  {
    MemoryLocker locker;
    locker.set_check_interval(::std::chrono::seconds(1));
    locker.set_filter([this](StringView path) {
      return nullptr == strstr(path.data(), file_name.c_str());
    });
    ASSERT_EQ(0, locker.start());
    ASSERT_FALSE(region_in_memory());
  }
  ASSERT_TRUE(region_in_memory());
}

#endif // !BABYLON_HAS_ADDRESS_SANITIZER and !ABSL_HAVE_THREAD_SANITIZER
