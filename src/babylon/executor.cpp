#include "babylon/executor.h"

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// Executor begin
Executor::~Executor() noexcept {}

int Executor::invoke(MoveOnlyFunction<void(void)>&&) noexcept {
  return -1;
}
// Executor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// InplaceExecutor begin
InplaceExecutor& InplaceExecutor::instance() noexcept {
  static InplaceExecutor executor;
  return executor;
}

InplaceExecutor::InplaceExecutor(bool flatten) noexcept : _flatten(flatten) {}

int InplaceExecutor::invoke(MoveOnlyFunction<void(void)>&& function) noexcept {
  if (!_flatten) {
    function();
    return 0;
  }

  if (_in_execution) {
    _pending_functions.emplace_back(::std::move(function));
    return 0;
  }

  MoveOnlyFunction<void(void)> next_function = ::std::move(function);
  while (true) {
    _in_execution = true;
    next_function();
    _in_execution = false;
    if (_pending_functions.empty()) {
      break;
    }
    next_function = ::std::move(_pending_functions.back());
    _pending_functions.pop_back();
  }
  return 0;
}
// InplaceExecutor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// AlwaysUseNewThreadExecutor begin
AlwaysUseNewThreadExecutor& AlwaysUseNewThreadExecutor::instance() noexcept {
  static AlwaysUseNewThreadExecutor executor;
  return executor;
}

int AlwaysUseNewThreadExecutor::invoke(
    MoveOnlyFunction<void(void)>&& function) noexcept {
  ::std::thread(::std::bind(
                    [](MoveOnlyFunction<void(void)>& function) {
                      function();
                    },
                    ::std::move(function)))
      .detach();
  return 0;
}
// AlwaysUseNewThreadExecutor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ThreadPoolExecutor begin
ThreadPoolExecutor::~ThreadPoolExecutor() noexcept {
  stop();
}

int ThreadPoolExecutor::initialize(size_t worker_num,
                                   size_t queue_capacity) noexcept {
  if (!_threads.empty()) {
    return -1;
  }
  _queue.reserve_and_clear(queue_capacity);
  _threads.reserve(worker_num);
  for (size_t i = 0; i < worker_num; ++i) {
    _threads.emplace_back(&ThreadPoolExecutor::keep_execute, this);
  }
  return 0;
}

void ThreadPoolExecutor::stop() noexcept {
  for (size_t i = 0; i < _threads.size(); ++i) {
    _queue.push<true, false, true>(MoveOnlyFunction<void(void)> {});
  }
  for (auto& thread : _threads) {
    thread.join();
  }
  _threads.clear();
}

int ThreadPoolExecutor::invoke(
    MoveOnlyFunction<void(void)>&& function) noexcept {
  _queue.push<true, false, true>(::std::move(function));
  return 0;
}

void ThreadPoolExecutor::keep_execute() noexcept {
  while (true) {
    MoveOnlyFunction<void(void)> function;
    _queue.pop<true, true, false>(function);
    if (function) {
      function();
    } else {
      break;
    }
  }
}
// ThreadPoolExecutor end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
