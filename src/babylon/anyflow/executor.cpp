#include "babylon/anyflow/executor.h"

#include "babylon/anyflow/vertex.h"

#include <tuple>

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

////////////////////////////////////////////////////////////////////////////////
Closure InplaceGraphExecutor::create_closure() noexcept {
  return Closure::create<SchedInterface>(*this);
}

int InplaceGraphExecutor::run(GraphVertex* vertex,
                              GraphVertexClosure&& closure) noexcept {
  vertex->run(::std::move(closure));
  return 0;
}

int InplaceGraphExecutor::run(ClosureContext* closure,
                              Closure::Callback* callback) noexcept {
  closure->run(callback);
  return 0;
}

InplaceGraphExecutor& InplaceGraphExecutor::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static InplaceGraphExecutor instance;
#pragma GCC diagnostic pop
  return instance;
}
////////////////////////////////////////////////////////////////////////////////

int ThreadPoolGraphExecutor::initialize(size_t worker_num,
                                        size_t queue_capacity) noexcept {
  _executor.set_worker_number(worker_num);
  _executor.set_global_capacity(queue_capacity);
  return _executor.start();
}

void ThreadPoolGraphExecutor::stop() noexcept {
  _executor.stop();
}

Closure ThreadPoolGraphExecutor::create_closure() noexcept {
  return Closure::create<SchedInterface>(*this);
}

int ThreadPoolGraphExecutor::run(GraphVertex* vertex,
                                 GraphVertexClosure&& closure) noexcept {
  return _executor.submit([captured_closure = ::std::move(closure), vertex]() mutable {
    vertex->run(::std::move(captured_closure));
  });
}

int ThreadPoolGraphExecutor::run(ClosureContext* closure,
                                 Closure::Callback* callback) noexcept {
  return _executor.submit([=] {
    closure->run(callback);
  });
}

} // namespace anyflow
BABYLON_NAMESPACE_END
