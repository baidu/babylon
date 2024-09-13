#pragma once

#include "babylon/environment.h"

BABYLON_NAMESPACE_BEGIN

class Executor;
class CurrentExecutor {
 public:
  inline static Executor* get() noexcept {
    return storage();
  }

  inline static void set(Executor* executor) noexcept {
    storage() = executor;
  }

 private:
  inline static Executor*& storage() noexcept {
    static thread_local Executor* executor {nullptr};
    return executor;
  }
};

BABYLON_NAMESPACE_END
