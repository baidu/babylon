#pragma once

#include "babylon/concurrent/bounded_queue.h"
#include "babylon/future.h"
#include "babylon/move_only_function.h"

#include <thread>
#include <vector>

BABYLON_NAMESPACE_BEGIN

// 抽象执行器基类
class Executor {
 public:
  virtual ~Executor() noexcept = 0;

  // 使用执行器执行一个任务
  template <typename C, typename... Args>
  inline int submit(C&& callable, Args&&... args) noexcept;

  // 使用执行器执行一个任务
  // 仿std::async接口，通过Future交互
  template <typename F = SchedInterface, typename C, typename... Args>
  inline Future<typename InvokeResult<C, Args...>::type, F> execute(
      C&& callable, Args&&... args) noexcept;

 protected:
  // 实际线程池实现者，只要在自己的【线程】中执行function即可
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept;

 private:
  // 成员函数使用std::mem_fn包装后才能和其他C等价处理
  template <typename C,
            typename ::std::enable_if<
                ::std::is_member_function_pointer<C>::value, int>::type = 0>
  inline static auto normalize(C&& callable) noexcept
      -> decltype(::std::mem_fn(callable));

  // 其他C不需要包装
  template <typename C,
            typename ::std::enable_if<
                !::std::is_member_function_pointer<C>::value, int>::type = 0>
  inline static C&& normalize(C&& callable) noexcept;

  // 执行function并对Promise进行set_value
  template <
      typename R, typename F, typename C, typename... Args,
      typename ::std::enable_if<!::std::is_same<void, R>::value, int>::type = 0>
  inline static void run_and_set(Promise<R, F>& promise, C&& callable,
                                 Args&&... args) noexcept;

  // void特化
  template <typename F, typename C, typename... Args>
  inline static void run_and_set(Promise<void, F>& promise, C&& callable,
                                 Args&&... args) noexcept;
};

// 仿照std::async的函数版本，executor替代了内置的policy机制，可用户扩展
template <typename F = SchedInterface, typename C, typename... Args>
inline Future<typename InvokeResult<C, Args...>::type, F> async(
    Executor& executor, C&& callable, Args&&... args) noexcept {
  return executor.execute<F>(::std::forward<C>(callable),
                             ::std::forward<Args>(args)...);
}

// 在当前线程原地运行的Executor
struct InplaceExecutor : public Executor {
 public:
  // 获取全局单例
  static InplaceExecutor& instance() noexcept;

  InplaceExecutor() = default;
  explicit InplaceExecutor(bool flatten) noexcept;

 protected:
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override;

 private:
  bool _flatten {false};
  bool _in_execution {false};
  ::std::vector<MoveOnlyFunction<void(void)>> _pending_functions;
};

// 永远新起线程的Executor，对齐std::async的默认行为
struct AlwaysUseNewThreadExecutor : public Executor {
 public:
  // 获取全局单例
  static AlwaysUseNewThreadExecutor& instance() noexcept;

 protected:
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override;
};

// 转交给一组线程池运行的Executor
class ThreadPoolExecutor : public Executor {
 public:
  // 析构自动停止运行
  virtual ~ThreadPoolExecutor() noexcept override;

  // 初始化
  // worker_num: 线程数
  // queue_capacity: 中转队列容量
  int initialize(size_t worker_num, size_t queue_capacity) noexcept;

  // 停止运行
  void stop() noexcept;

 protected:
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override;

 private:
  void keep_execute() noexcept;

  ConcurrentBoundedQueue<MoveOnlyFunction<void(void)>> _queue;
  ::std::vector<::std::thread> _threads;
};

BABYLON_NAMESPACE_END

#include "babylon/executor.hpp"
