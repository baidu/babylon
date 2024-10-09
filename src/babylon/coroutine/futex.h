#pragma once

#include "babylon/concurrent/deposit_box.h"
#include "babylon/coroutine/promise.h"

#include <mutex>

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

class Futex {
 public:
  class Awaitable;

  inline Awaitable wait(uint64_t expected_value, const struct ::timespec* timeout) noexcept;
  inline int wake_one() noexcept;
  inline int wake_all() noexcept;

 private:

  struct Node;

  ::std::mutex _mutex;
  uint64_t _value {0};

  Node* _awaiter_head;
};

class Futex::Awaitable {
 public:
 private:
  Node* _node {nullptr};
};

struct Futex::Node {
  Node* prev {nullptr};
  Node* next {nullptr};
};

inline Futex::Awaitable Futex::wait(uint64_t expected_value, const struct ::timespec*) noexcept {

  {
    ::std::lock_guard<::std::mutex> lock {_mutex};
    if (expected_value != _value) {
      return {};
    }
  }
  return {};
}

BABYLON_COROUTINE_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
