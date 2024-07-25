#pragma once

#include "babylon/executor.h"
#include "babylon/logging/logger.h"

#include "bthread/bthread.h"
#include "butex_interface.h"

BABYLON_NAMESPACE_BEGIN

// 使用bthread线程池执行的executor
class BthreadExecutor : public Executor {
 public:
  inline static BthreadExecutor& instance() noexcept {
    static BthreadExecutor bthread_executor;
    return bthread_executor;
  }

 protected:
  // 启动bthread，并运行function
  virtual int invoke(
      MoveOnlyFunction<void(void)>&& function) noexcept override {
    auto args = new MoveOnlyFunction<void(void)>(::std::move(function));
    ::bthread_t th;
    if (0 != ::bthread_start_background(&th, NULL, run_function, args)) {
      BABYLON_LOG(WARNING) << "start bthread to execute failed";
      function = ::std::move(*args);
      delete args;
      return -1;
    }
    return 0;
  }

 private:
  // 线程函数
  inline static void* run_function(void* args) noexcept {
    auto function = reinterpret_cast<MoveOnlyFunction<void(void)>*>(args);
    (*function)();
    delete function;
    return nullptr;
  }
};

// 使用bthread线程池的async
template <typename S = ::babylon::ButexInterface, typename C, typename... Args>
inline Future<typename ::std::result_of<typename ::std::decay<C>::type(
                  typename ::std::decay<Args>::type...)>::type,
              S>
bthread_async(C&& callable, Args&&... args) noexcept {
  return BthreadExecutor::instance().execute<S>(::std::forward<C>(callable),
                                                ::std::forward<Args>(args)...);
}

BABYLON_NAMESPACE_END
