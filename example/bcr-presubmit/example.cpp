#include "babylon/future.h"
#include "babylon/logging/logger.h"

#include <thread>
#include <vector>

int main() {
  ::babylon::CountDownLatch<> latch(10);
  auto future = latch.get_future();
  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < 10; ++i) {
    threads.emplace_back([&, i]() {
      BABYLON_LOG(INFO) << "finish " << i;
      latch.count_down();
    });
  }
  future.get();
  BABYLON_LOG(INFO) << "finish all";
  for (auto& thread : threads) {
    thread.join();
  }
  return 0;
}
