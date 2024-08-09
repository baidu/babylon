#include "babylon/logging/rolling_file_object.h"

#if __cplusplus >= 201703L
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>

using ::babylon::FileObject;
using ::babylon::RollingFileObject;

struct RollingFileObjectTest : public ::testing::Test {
  virtual void SetUp() override {
    directory = "log_" + ::std::to_string(getpid());
    ::std::filesystem::remove_all(directory);
    rolling_object.set_directory(directory);
    rolling_object.set_file_pattern("name.%Y%m%d-%H%M%S");
    object = &rolling_object;
  }

  virtual void TearDown() override {
    ::std::filesystem::remove_all(directory);
  }

  ::std::string directory;
  RollingFileObject rolling_object;
  FileObject* object;
};

TEST_F(RollingFileObjectTest, file_first_create_when_get_trigger) {
  object->check_and_get_file_descriptor();
  {
    ::std::filesystem::directory_iterator dir {directory};
    ASSERT_EQ(1, ::std::distance(begin(dir), end(dir)));
  }
}

TEST_F(RollingFileObjectTest, keep_file_dont_exceed_num) {
  ::std::filesystem::create_directory(directory);
  ::std::ofstream {directory + "/name.00000000-000000"};

  ::std::vector<int> old_fds;
  rolling_object.set_max_file_number(3);
  rolling_object.scan_and_tracking_existing_files();
  for (size_t i = 0; i < 50; ++i) {
    auto [fd, old_fd] = object->check_and_get_file_descriptor();
    if (old_fd >= 0) {
      old_fds.emplace_back(old_fd);
    }
    rolling_object.delete_expire_files();
    ::usleep(100 * 1000);
  }

  {
    ::std::filesystem::directory_iterator dir {directory};
    ASSERT_EQ(3, ::std::distance(begin(dir), end(dir)));
  }

  ASSERT_LE(4, old_fds.size());
  for (auto fd : old_fds) {
    ::close(fd);
  }
}

TEST_F(RollingFileObjectTest, fd_refer_to_latest_file) {
  int last_fd = -1;
  ::std::vector<int> old_fds;
  rolling_object.set_max_file_number(1);
  for (size_t i = 0; i < 30; ++i) {
    auto [fd, old_fd] = object->check_and_get_file_descriptor();
    if (old_fd >= 0) {
      old_fds.emplace_back(old_fd);
    }
    rolling_object.delete_expire_files();
    last_fd = fd;
    ::usleep(100 * 1000);
  }

  ASSERT_NE(-1, last_fd);
  ASSERT_LT(0, ::dprintf(last_fd, "this should appear in file\n"));
  ::std::filesystem::directory_iterator dir {directory};
  auto entry = *begin(dir);
  ::std::ifstream ifs {entry.path()};
  ASSERT_TRUE(ifs);
  ::std::string line;
  ASSERT_TRUE(::std::getline(ifs, line));
  ASSERT_EQ("this should appear in file", line);
}
#endif // __cplusplus >= 201703L
