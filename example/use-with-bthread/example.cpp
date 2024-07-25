#include "babylon/future.h"
#include "babylon/logging/logger.h"

#include "bthread_executor.h"
#include "butex_interface.h"

#include <vector>

int main() {
  ::babylon::CountDownLatch<::babylon::ButexInterface> latch(10);
  auto future = latch.get_future();
  for (size_t i = 0; i < 10; ++i) {
    ::babylon::bthread_async([&, i]() {
      BABYLON_LOG(INFO) << "finish " << i;
      latch.count_down();
    });
  }
  future.get();
  BABYLON_LOG(INFO) << "finish all";
  return 0;
}
