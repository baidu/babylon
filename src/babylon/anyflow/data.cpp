#include "babylon/anyflow/data.h"

#include "babylon/anyflow/closure.h"
#include "babylon/anyflow/dependency.h"
#include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

void GraphData::set_name(const ::std::string& name) noexcept {
  _name = name;
}

void GraphData::set_graph(const Graph& graph) noexcept {
  _graph = &graph;
}

void GraphData::release() noexcept {
  ClosureContext* closure = _closure.load(::std::memory_order_relaxed);
  do {
    if (ABSL_PREDICT_FALSE(closure == SEALED_CLOSURE)) {
      return;
    }
  } while (ABSL_PREDICT_FALSE(!_closure.compare_exchange_weak(
      closure, SEALED_CLOSURE, ::std::memory_order_acq_rel)));

  if (ABSL_PREDICT_FALSE(nullptr != closure)) {
    closure->depend_data_sub();
  }

  VertexStack runnable_vertexes;
  auto trivial_runnable_vertexes =
      _producer != nullptr ? _producer->runnable_vertexes() : nullptr;

  if (trivial_runnable_vertexes == nullptr) {
    for (auto successor : _successors) {
      successor->ready(this, runnable_vertexes);
    }
  } else {
    for (auto successor : _successors) {
      successor->ready(this, *trivial_runnable_vertexes);
    }
  }
  while (!runnable_vertexes.empty()) {
    auto vertex = runnable_vertexes.back();
    runnable_vertexes.pop_back();
    vertex->invoke(runnable_vertexes);
  }
}

int GraphData::recursive_activate(VertexStack& runnable_vertexes,
                                  ClosureContext* closure) noexcept {
// TODO(lijiang01): InlinedVector实现中有些逻辑会引起GCC误报
#pragma GCC diagnostic push
#ifdef __GNUC__
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif
  DataStack activating_data;
  trigger(activating_data);
#pragma GCC diagnostic pop
  while (!activating_data.empty()) {
    GraphData* one_data = activating_data.back();
    activating_data.pop_back();
    if (0 != one_data->activate(activating_data, runnable_vertexes, closure)) {
      BABYLON_LOG(WARNING) << "activate " << *one_data << " failed";
      return -1;
    }
  }
  return 0;
}

int GraphData::activate(DataStack& activating_data,
                        VertexStack& runnable_vertexes,
                        ClosureContext* closure) noexcept {
  // trigger时检测过一次ready，送给activate的如果没有producer直接报错
  if (ABSL_PREDICT_FALSE(_producers.empty())) {
    BABYLON_LOG(WARNING) << "can not activate " << *this << " with no producer";
    return -1;
  }
  for (auto& producer : _producers) {
    if (ABSL_PREDICT_FALSE(0 != producer->activate(activating_data,
                                                   runnable_vertexes,
                                                   closure))) {
      BABYLON_LOG(WARNING) << "activate producer " << producer << " of "
                           << *this << " failed";
      return -1;
    }
  }
  return 0;
}

bool GraphData::check_safe_mutable() const noexcept {
  // 依赖不超过1个，是安全的
  if (_successors.size() <= 1) {
    return true;
  }
  // 如果有任何一个是无条件成立，且需要可变数据
  // 则表示运行时需求可变数据的一定超过了1个
  // 是不安全的
  for (auto dependency : _successors) {
    if (dependency->inner_condition() == nullptr && dependency->is_mutable()) {
      return false;
    }
  }
  // 无法静态判定为一定不安全，先放行
  // 运行时根据实际的条件状态，做进一步检测
  return true;
}

ClosureContext* GraphData::SEALED_CLOSURE =
    reinterpret_cast<ClosureContext*>(0xFFFFFFFFFFFFFFFFL);

} // namespace anyflow
BABYLON_NAMESPACE_END
