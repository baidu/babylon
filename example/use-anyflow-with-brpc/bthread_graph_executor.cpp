#include "bthread_graph_executor.h"

#include "babylon/logging/logger.h"

#include "butex_interface.h"

#include <tuple>

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

static void* execute_invoke_vertex(void* args) {
  auto param =
      reinterpret_cast<::std::tuple<GraphVertex*, GraphVertexClosure>*>(args);
  auto vertex = ::std::get<0>(*param);
  auto& closure = ::std::get<1>(*param);
  vertex->run(::std::move(closure));
  delete param;
  return NULL;
}

static void* execute_invoke_closure(void* args) {
  auto param =
      reinterpret_cast<::std::tuple<ClosureContext*, Closure::Callback*>*>(
          args);
  auto closure = ::std::get<0>(*param);
  auto callback = ::std::get<1>(*param);
  closure->run(callback);
  delete param;
  return NULL;
}

BthreadGraphExecutor& BthreadGraphExecutor::instance() {
  static BthreadGraphExecutor executor;
  return executor;
}

Closure BthreadGraphExecutor::create_closure() noexcept {
  return Closure::create<ButexInterface>(*this);
}

int BthreadGraphExecutor::run(GraphVertex* vertex,
                              GraphVertexClosure&& closure) noexcept {
  bthread_t th;
  auto param = new ::std::tuple<GraphVertex*, GraphVertexClosure>(
      vertex, ::std::move(closure));
  if (0 != bthread_start_background(&th, NULL, execute_invoke_vertex, param)) {
    LOG(WARNING) << "start bthread to run vertex failed";
    closure = ::std::move(::std::get<1>(*param));
    delete param;
    return -1;
  }
  return 0;
}

int BthreadGraphExecutor::run(ClosureContext* closure,
                              Closure::Callback* callback) noexcept {
  bthread_t th;
  auto param =
      new ::std::tuple<ClosureContext*, Closure::Callback*>(closure, callback);
  if (0 != bthread_start_background(&th, NULL, execute_invoke_closure, param)) {
    LOG(WARNING) << "start bthread to run closure failed";
    delete param;
    return -1;
  }
  return 0;
}

} // namespace anyflow
BABYLON_NAMESPACE_END
