#include "babylon/executor.h"

// clang-foramt off
#include "babylon/protect.h"
// clang-foramt on

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// Executor begin
// Executor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// InplaceExecutor begin
InplaceExecutor& InplaceExecutor::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static InplaceExecutor executor;
#pragma GCC diagnostic pop
  return executor;
}

int InplaceExecutor::invoke(MoveOnlyFunction<void(void)>&& function) noexcept {
  RunnerScope scope {*this};
  function();
  return 0;
}
// InplaceExecutor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// AlwaysUseNewThreadExecutor begin
AlwaysUseNewThreadExecutor& AlwaysUseNewThreadExecutor::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static AlwaysUseNewThreadExecutor executor;
#pragma GCC diagnostic pop
  return executor;
}

void AlwaysUseNewThreadExecutor::join() noexcept {
  while (_running.load(::std::memory_order_acquire) > 0) {
    ::usleep(1000);
  }
}

AlwaysUseNewThreadExecutor::~AlwaysUseNewThreadExecutor() noexcept {
  join();
}

int AlwaysUseNewThreadExecutor::invoke(
    MoveOnlyFunction<void(void)>&& function) noexcept {
  _running.fetch_add(1, ::std::memory_order_acq_rel);
  ::std::thread([this, captured_function = ::std::move(function)] {
    RunnerScope scope {*this};
    captured_function();
    _running.fetch_sub(1, ::std::memory_order_acq_rel);
  }).detach();
  return 0;
}
// AlwaysUseNewThreadExecutor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ThreadPoolExecutor begin
ThreadPoolExecutor::~ThreadPoolExecutor() noexcept {
  stop();
}

void ThreadPoolExecutor::set_worker_number(size_t worker_number) noexcept {
  _worker_number = worker_number;
}

void ThreadPoolExecutor::set_local_capacity(size_t local_capacity) noexcept {
  _local_capacity = local_capacity;
}

void ThreadPoolExecutor::set_global_capacity(size_t global_capacity) noexcept {
  _global_capacity = global_capacity;
}

void ThreadPoolExecutor::set_enable_work_stealing(
    bool enable_work_stealing) noexcept {
  _enable_work_stealing = enable_work_stealing;
}

int ThreadPoolExecutor::start() noexcept {
  if (_running.load(::std::memory_order_acquire)) {
    return -1;
  }
  _running.store(true, ::std::memory_order_release);
  _global_task_queue.reserve_and_clear(_global_capacity * 2);
  _local_task_queues.set_constructor(
      [this](ConcurrentBoundedQueue<Task>* queue) {
        new (queue) ConcurrentBoundedQueue<Task>;
        queue->reserve_and_clear(_local_capacity * 2);
      });
  _threads.reserve(_worker_number);
  for (size_t i = 0; i < _worker_number; ++i) {
    _threads.emplace_back(&ThreadPoolExecutor::keep_execute, this);
  }
  if (_balance_interval.count() >= 0) {
    _balance_thread = ::std::thread(&ThreadPoolExecutor::keep_balance, this);
  }
  return 0;
}

void ThreadPoolExecutor::wakeup_one_worker() noexcept {
  _global_task_queue.push<true, false, true>(
      Task {.type = TaskType::WAKEUP, .function {}});
}

void ThreadPoolExecutor::stop() noexcept {
  if (!_running.load(::std::memory_order_acquire)) {
    return;
  }
  _running.store(false, ::std::memory_order_release);
  if (_balance_thread.joinable()) {
    _balance_thread.join();
  }
  for (size_t i = 0; i < _threads.size(); ++i) {
    _global_task_queue.push<true, false, true>(
        Task {.type = TaskType::STOP, .function {}});
  }
  for (auto& thread : _threads) {
    thread.join();
  }
  _threads.clear();
}

int ThreadPoolExecutor::initialize(size_t worker_number,
                                   size_t global_capacity) noexcept {
  if (!_threads.empty()) {
    return -1;
  }
  _worker_number = worker_number;
  _global_capacity = global_capacity;
  return start();
}

int ThreadPoolExecutor::invoke(
    MoveOnlyFunction<void(void)>&& function) noexcept {
  return enqueue_task(
      {.type = TaskType::FUNCTION, .function {::std::move(function)}});
}

void ThreadPoolExecutor::keep_execute() noexcept {
  auto& local_queue = _local_task_queues.local();
  RunnerScope scope {*this};
  while (true) {
    Task task;
    if (!local_queue.try_pop<true, false>(task)) {
      bool steal_success = false;
      if (_enable_work_stealing) {
        _local_task_queues.for_each([&](TaskQueue* iter, TaskQueue* end) {
          if (steal_success) {
            return;
          }
          while (iter != end) {
            auto& queue = *iter++;
            steal_success = queue.try_pop<true, false>(task);
            if (steal_success) {
              return;
            }
          }
        });
      }
      if (!steal_success) {
        _global_task_queue.pop<true, true, false>(task);
      }
    }
    switch (task.type) {
      case TaskType::FUNCTION: {
        task.function();
      } break;
      case TaskType::STOP: {
        return;
      }
      case TaskType::WAKEUP: {
      } break;
      default:
        assert(false);
    }
  }
}

void ThreadPoolExecutor::keep_balance() noexcept {
  while (_running.load(::std::memory_order_acquire)) {
    ::std::this_thread::sleep_for(_balance_interval);

    _local_task_queues.for_each([&](TaskQueue* iter, TaskQueue* end) {
      while (iter != end) {
        auto& queue = *iter++;
        bool success = true;
        while (success) {
          success = queue.try_pop<true, false>([&](Task& task) {
            enqueue_task(::std::move(task));
          });
        }
      }
    });
  }
}

int ThreadPoolExecutor::enqueue_task(Task&& task) noexcept {
  if (is_running_in()) {
    if (_local_capacity > 0) {
      auto& local_queue = _local_task_queues.local();
      if (local_queue.size() < _local_capacity) {
        local_queue.push<false, false, false>(::std::move(task));
        return 0;
      }
    }
  }
  _global_task_queue.push<true, false, true>(::std::move(task));
  return 0;
}
// ThreadPoolExecutor end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
