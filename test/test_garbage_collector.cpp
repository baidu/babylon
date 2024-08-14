#include "babylon/garbage_collector.h"

#include "gtest/gtest.h"

#include <future>

using ::babylon::GarbageCollector;

struct GarbageCollectorTest : public ::testing::Test {
  GarbageCollector gc;
};

TEST_F(GarbageCollectorTest, lock_return_version_current_locked) {
  auto accessor = gc.create_accessor();
  ::std::unique_lock<GarbageCollector::Accessor> lock(accessor);
}
