#include "babylon/anyflow/vertex.h"

#include "babylon/anyflow/executor.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

////////////////////////////////////////////////////////////////////////////////
// GraphProcessor begin
GraphProcessor::~GraphProcessor() noexcept {}

int GraphProcessor::config(const ::babylon::Any& origin_option,
                           ::babylon::Any& option) const noexcept {
  option.ref(origin_option);
  return 0;
}

int GraphProcessor::setup(GraphVertex& vertex) noexcept {
  _vertex = &vertex;
  if (0 != __anyflow_declare_interface()) {
    BABYLON_LOG(WARNING) << "ANYFLOW_INTERFACE declare failed for " << vertex;
    return -1;
  }
  if (0 != setup()) {
    BABYLON_LOG(WARNING) << "setup failed for " << vertex;
    return -1;
  }
  return 0;
}

int GraphProcessor::__anyflow_declare_interface() noexcept {
  return 0;
}

int GraphProcessor::setup() noexcept {
  return 0;
}

int GraphProcessor::on_activate() noexcept {
  return 0;
}

void GraphProcessor::process(GraphVertex& vertex,
                             GraphVertexClosure&& closure) noexcept {
  _vertex = &vertex;
  if (ABSL_PREDICT_FALSE(0 != __anyflow_prepare_interface())) {
    BABYLON_LOG(WARNING) << "ANYFLOW_INTERFACE prepare failed for " << vertex;
    closure.done(-1);
    return;
  }
  process(::std::move(closure));
}

void GraphProcessor::reset(GraphVertex& vertex) noexcept {
  _vertex = &vertex;
  reset();
}

int GraphProcessor::__anyflow_prepare_interface() noexcept {
  return 0;
}

void GraphProcessor::process(GraphVertexClosure&& closure) noexcept {
  closure.done(process());
}

int GraphProcessor::process(GraphVertex& vertex) noexcept {
  _vertex = &vertex;
  if (ABSL_PREDICT_FALSE(0 != __anyflow_prepare_interface())) {
    BABYLON_LOG(WARNING) << "ANYFLOW_INTERFACE prepare failed for " << vertex;
    return -1;
  }
  return process();
}

int GraphProcessor::process() noexcept {
  return 0;
}

void GraphProcessor::reset() noexcept {}
// GraphProcessor end
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GraphVertex begin
GraphDependency::GraphDependency(GraphDependency&& other) noexcept
    : _source(other._source),
      _target(other._target),
      _condition(other._condition),
      _establish_value(other._establish_value),
      _mutable(other._mutable),
      _waiting_num(other._waiting_num.load()),
      _established(other._established),
      _ready(other._ready),
      _essential(other._essential) {
  ::std::swap(_source, other._source);
  ::std::swap(_target, other._target);
  ::std::swap(_condition, other._condition);
  ::std::swap(_establish_value, other._establish_value);
  ::std::swap(_mutable, other._mutable);
  auto tmp = _waiting_num.load();
  _waiting_num.store(other._waiting_num.load());
  other._waiting_num.store(tmp);
  ::std::swap(_established, other._established);
  ::std::swap(_ready, other._ready);
  ::std::swap(_essential, other._essential);
}

ssize_t GraphVertex::index_for_named_dependency(
    ::babylon::StringView name) noexcept {
  return _builder->index_for_named_dependency(name);
}

ssize_t GraphVertex::index_for_named_emit(::babylon::StringView name) noexcept {
  return _builder->index_for_named_emit(name);
}

void GraphVertex::declare_trivial() noexcept {
  _trivial = true;
}

void GraphVertex::flush_emits() noexcept {
  for (auto data : emits()) {
    if (!data->ready()) {
      data->emit<::babylon::Any>();
    }
  }
}

void GraphVertex::set_graph(Graph& graph) noexcept {
  _graph = &graph;
}

void GraphVertex::set_builder(const GraphVertexBuilder& builder) noexcept {
  _builder = &builder;
}

void GraphVertex::set_processor(
    ::std::unique_ptr<GraphProcessor>&& processor) noexcept {
  _processor = ::std::move(processor);
}

::std::vector<GraphDependency>& GraphVertex::dependencies() noexcept {
  return _dependencies;
}

::std::vector<GraphData*>& GraphVertex::emits() noexcept {
  return _emits;
}

int GraphVertex::setup() noexcept {
  return _processor->setup(*this);
}

void GraphVertex::reset() noexcept {
  _activated.store(false, ::std::memory_order_relaxed);
  _waiting_num.store(0, ::std::memory_order_relaxed);
  _closure = nullptr;
  for (auto& denpendency : _dependencies) {
    denpendency.reset();
  }
  _runnable_vertexes = nullptr;
  _processor->reset(*this);
}

int GraphVertex::activate(DataStack& activating_data,
                          VertexStack& runnable_vertexes,
                          ClosureContext* closure) noexcept {
  // 只能激活一次
  bool expected = false;
  if (!_activated.compare_exchange_strong(expected, true,
                                          ::std::memory_order_relaxed)) {
    return 0;
  }

  // 记录激活vertex的closure
  _closure = closure;

  // 无依赖直接记入可执行列表
  auto waiting_num = _dependencies.size();
  if (waiting_num == 0) {
    runnable_vertexes.emplace_back(this);
    return 0;
  }
  _waiting_num.store(static_cast<int64_t>(waiting_num),
                     ::std::memory_order_relaxed);

  if (0 != _processor->on_activate()) {
    return -1;
  }

  // 激活每个依赖，记录激活时已经就绪的数目
  int64_t finished = 0;
  for (auto& dependency : _dependencies) {
    int64_t ret = dependency.activate(activating_data);
    if (ABSL_PREDICT_FALSE(ret < 0)) {
      return ret;
    }
    finished += ret;
  }

  // 去掉已经就绪的数目，如果全部就绪，节点加入待运行集合
  if (finished > 0) {
    waiting_num = static_cast<size_t>(
        _waiting_num.fetch_sub(finished, ::std::memory_order_acq_rel) -
        finished);
    if (waiting_num == 0) {
      runnable_vertexes.emplace_back(this);
      return 0;
    }
  }

  return 0;
}

void GraphVertex::invoke(VertexStack& runnable_vertexes) noexcept {
  bool essential_failed = false;
  for (auto& dependency : _dependencies) {
    if (dependency.is_essential() &&
        (!dependency.ready() || dependency.empty())) {
      essential_failed = true;
      break;
    }
  }
  if (!essential_failed) {
    if (_trivial) {
      // todo: emit中可以记录一下是否来自trivial的vertex
      // 否则trivial的vertex不支持外部并发注入data
      // 虽然一般没这种情况，不过可以更完备
      _runnable_vertexes = &runnable_vertexes;
      run(GraphVertexClosure(*_closure, *this));
    } else {
      GraphVertexClosure closure(*_closure, *this);
      if (ABSL_PREDICT_FALSE(0 != _builder->graph().executor().run(this,
                                                                   ::std::move(closure)))) {
        // executor.run 返回失败时，closure 可能已被 move，需要重新创建来标记错误
        GraphVertexClosure error_closure(*_closure, *this);
        error_closure.done(-1);
      }
    }
  } else {
    // 不允许平凡模式情况下，不设置_runnable_vertexes
    if (_builder->allow_trivial()) {
      _runnable_vertexes = &runnable_vertexes;
    }
    // 不运行算子，直接发布emits
    flush_emits();
  }
}

} // namespace anyflow
BABYLON_NAMESPACE_END
