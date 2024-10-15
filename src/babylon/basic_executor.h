#pragma once

#include "babylon/move_only_function.h" // MoveOnlyFunction

BABYLON_COROUTINE_NAMESPACE_BEGIN
class BasicPromise;
BABYLON_COROUTINE_NAMESPACE_END

BABYLON_NAMESPACE_BEGIN

// Unified interface about an asynchronous execution mechanism.
//
// **NOT** designed to be directly used or derived by custom implementations.
// Use babylon::Executor instead, which is more like a **base** executor class
// both to user and developer.
//
// This basic part is extracted out from babylon::Executor mainly to eliminate
// circular dependency between babylon::Executor and babylon::CoroutineTask.
class BasicExecutor {
 public:
  // RAII scope for mark current running in some executor.
  class RunnerScope;

  BasicExecutor() noexcept = default;
  BasicExecutor(BasicExecutor&&) noexcept = default;
  BasicExecutor(const BasicExecutor&) noexcept = default;
  BasicExecutor& operator=(BasicExecutor&&) noexcept = default;
  BasicExecutor& operator=(const BasicExecutor&) noexcept = default;

  inline bool is_running_in() const noexcept;

 private:
  // Every execution will be packed into a type-erased closure function,
  // and call this interface finally. A reasonable implementation is expecetd to
  // move function to some destination thread-like worker and run it there.
  //
  // return ==0: Front-end transfer is success. The function will be called
  // later always.
  //        !=0: Front-end transfer is fail. The function is not moved away, and
  //        never be called inside.
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept;

 private:
  static BasicExecutor*& current() noexcept;

  virtual ~BasicExecutor() noexcept;

  friend coroutine::BasicPromise; // invoke
  friend class Executor;          // inherited by Executor only
};

class BasicExecutor::RunnerScope {
 public:
  RunnerScope() = delete;
  RunnerScope(RunnerScope&&) = delete;
  RunnerScope(const RunnerScope&) = delete;
  RunnerScope& operator=(RunnerScope&&) = delete;
  RunnerScope& operator=(const RunnerScope&) = delete;
  inline ~RunnerScope() noexcept;

  inline RunnerScope(BasicExecutor& current) noexcept;

 private:
  BasicExecutor* _old_current;

  friend BasicExecutor;
};

////////////////////////////////////////////////////////////////////////////////
// BasicExecutor::RunnerScope begin
inline BasicExecutor::RunnerScope::RunnerScope(
    BasicExecutor& new_current) noexcept
    : _old_current {BasicExecutor::current()} {
  BasicExecutor::current() = &new_current;
}

inline BasicExecutor::RunnerScope::~RunnerScope() noexcept {
  BasicExecutor::current() = _old_current;
}
// BasicExecutor::RunnerScope end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BasicExecutor begin
inline bool BasicExecutor::is_running_in() const noexcept {
  return this == current();
}
// BasicExecutor end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
