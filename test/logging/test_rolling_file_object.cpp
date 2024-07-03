#include "babylon/logging/rolling_file_object.h"

#include "gtest/gtest.h"

using ::babylon::RollingFileObject;

struct RollingFileObjectTest : public ::testing::Test {
  virtual void SetUp() override {}

  virtual void TearDown() override {}
};

TEST_F(RollingFileObjectTest, tttt) {
  RollingFileObject object;
  object.set_directory("log");
  object.set_file_pattern("name.%Y%m%d-%H%M%S");
  object.set_max_file_number(5);

  object.initialize();

  for (size_t i = 0; i < 5000; ++i) {
    ((::babylon::FileObject&)object).check_and_get_file_descriptor();
    usleep(1000);
  }
}
