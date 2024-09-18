#include "babylon/coroutine.h"

#include "babylon/executor.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_NAMESPACE_BEGIN

// We cannot call function of executor in .h because it incomplete there. So
// functions need interact with executor are placed in .cpp here.
void BasicCoroutinePromise::resume_awaiter() noexcept {
  _awaiter_executor->resume(_awaiter);
}

void BasicCoroutinePromise::resume(::std::coroutine_handle<> handle) noexcept {
  _executor->resume(handle);
}

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
