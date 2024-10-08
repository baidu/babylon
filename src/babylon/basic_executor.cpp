#include "babylon/basic_executor.h"

// clang-foramt off
#include "babylon/protect.h"
// clang-foramt on

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// BasicExecutor begin
int BasicExecutor::invoke(MoveOnlyFunction<void(void)>&&) noexcept {
  return -1;
}

BasicExecutor*& BasicExecutor::current() noexcept {
  static thread_local BasicExecutor* executor;
  return executor;
}

BasicExecutor::~BasicExecutor() noexcept {}
// BasicExecutor end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
