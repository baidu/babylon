#include "babylon/coroutine.h"

#include "babylon/executor.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_NAMESPACE_BEGIN

// We cannot call function of executor in .h because it incomplete there. So
// functions need interact with executor are placed in .cpp here.

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
