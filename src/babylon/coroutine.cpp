#include "babylon/coroutine.h"

#include "babylon/executor.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_NAMESPACE_BEGIN

void BasicCoroutinePromise::resume_awaiter() noexcept {
  BABYLON_LOG(INFO) << "BasicCoroutinePromise::resume_awaiter address "
                    << _awaiter.address() << " to " << _awaiter_executor;
  _awaiter_executor->resume(_awaiter);
}

void BasicCoroutinePromise::resume(::std::coroutine_handle<> handle) noexcept {
  BABYLON_LOG(INFO) << "BasicCoroutinePromise::resume address "
                    << handle.address() << " to " << _executor;
  _executor->resume(handle);
}

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
