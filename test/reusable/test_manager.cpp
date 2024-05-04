#include "babylon/reusable/manager.h"
#include "babylon/reusable/string.h"

#include "gtest/gtest.h"

using ::babylon::ReusableAccessor;
using ::babylon::ReusableManager;
using ::babylon::SwissMemoryResource;
using ::babylon::SwissString;
using ::babylon::SystemPageAllocator;

struct ReusableManagerTest : public ::testing::Test {
  ReusableManager<SwissMemoryResource> manager;
  ::std::string long_string {::std::string(1024, 'x')};
};

TEST_F(ReusableManagerTest, underlying_resouce_configurable) {
  manager.resource().set_page_allocator(SystemPageAllocator::instance());
}

TEST_F(ReusableManagerTest, instance_create_on_resource) {
  auto s = manager.create_object<SwissString>(long_string);
  ASSERT_EQ(long_string, *s);
  ASSERT_TRUE(manager.resource().contains(&*s));
  ASSERT_TRUE(manager.resource().contains(s->c_str()));
}

TEST_F(ReusableManagerTest, clear_all_instance) {
  manager.resource().allocate(512, 8);
  auto s1 = manager.create_object<SwissString>(long_string);
  auto s2 = manager.create_object<SwissString>(long_string);
  auto p1 = s1->c_str();
  auto p2 = s2->c_str();
  ASSERT_EQ(long_string, *s1);
  ASSERT_EQ(long_string, *s2);
  manager.clear();
  ASSERT_EQ(p1, s1->c_str());
  ASSERT_TRUE(s1->empty());
  ASSERT_LE(long_string.size(), s1->capacity());
  ASSERT_EQ(p2, s2->c_str());
  ASSERT_TRUE(s2->empty());
  ASSERT_LE(long_string.size(), s2->capacity());
}

TEST_F(ReusableManagerTest, recreate_instance_with_capacity) {
  manager.set_recreate_interval(5);
  manager.resource().allocate(512, 8);
  auto s1 = manager.create_object<SwissString>(long_string);
  auto s2 = manager.create_object<SwissString>(long_string);
  auto p1 = s1->c_str();
  ASSERT_EQ(long_string, *s1);
  ASSERT_EQ(long_string, *s2);
  size_t times = 0;
  while (p1 == s1->c_str()) {
    manager.clear();
    times++;
  }
  ASSERT_EQ(5, times);
  ASSERT_TRUE(s1->empty());
  ASSERT_LE(long_string.size(), s1->capacity());
  ASSERT_TRUE(s2->empty());
  ASSERT_LE(long_string.size(), s2->capacity());
}
