#include "babylon/coroutine.h"

#include "babylon/executor.h"

BABYLON_NAMESPACE_BEGIN

#if __cpp_concepts && __cpp_lib_coroutine
void BasicCoroutinePromise::resume(::std::coroutine_handle<> handle) noexcept {
  _executor->resume(handle);
}
#endif // __cpp_concepts && __cpp_lib_coroutine

BABYLON_NAMESPACE_END
