#include "babylon/anyflow/builder.h"

#include "babylon/anyflow/data.h"
#include "babylon/anyflow/graph.h"
#include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

////////////////////////////////////////////////////////////////////////////////
// GraphBuilder begin
GraphBuilder& GraphBuilder::set_name(::babylon::StringView name) noexcept {
  _name = name;
  return *this;
}

GraphBuilder& GraphBuilder::set_executor(GraphExecutor& executor) noexcept {
  _executor = &executor;
  return *this;
}

int GraphBuilder::finish() noexcept {
  _producers_for_data_index.clear();
  _data_index_for_name.clear();
  for (auto& vertex : _vertexes) {
    if (0 != vertex->finish()) {
      BABYLON_LOG(WARNING) << "finish " << *vertex << " failed";
      return -1;
    }
  }
  for (auto& pair : _producers_for_data_index) {
    auto& producers = pair.second;
    if (producers.size() > 1) {
      for (auto vertex : producers) {
        vertex->set_allow_trivial(false);
      }
    }
  }
  return 0;
}

::std::unique_ptr<Graph> GraphBuilder::build() const noexcept {
  ::std::unique_ptr<Graph> graph {new Graph};
  graph->set_executor(*_executor);
  graph->set_page_allocator(*_page_allocator);
  graph->initialize_data(_data_index_for_name);
  graph->initialize_vertexes(_vertexes.size());
  auto& vertexes = graph->vertexes();

  for (size_t i = 0; i < _vertexes.size(); ++i) {
    auto& builder = _vertexes[i];
    auto& vertex = vertexes[i];
    if (0 != builder->build(*graph, vertex)) {
      BABYLON_LOG(WARNING) << "build " << *builder << " failed";
      return ::std::unique_ptr<Graph>();
    }
  }

  for (const auto& one_data : graph->data()) {
    if (ABSL_PREDICT_FALSE(0 != one_data.error_code())) {
      BABYLON_LOG(WARNING) << one_data << " build failed";
      return ::std::unique_ptr<Graph>();
    }
    if (ABSL_PREDICT_FALSE(!one_data.check_safe_mutable())) {
      BABYLON_LOG(WARNING) << one_data << " mutable but non exclusive";
      return ::std::unique_ptr<Graph>();
    }
  }
  return graph;
}

size_t GraphBuilder::get_or_allocate_data_index(StringView name) noexcept {
  size_t data_index = _data_index_for_name.size();
  auto result = _data_index_for_name.emplace(name, data_index);
  if (!result.second) {
    data_index = result.first->second;
  }
  return data_index;
}

void GraphBuilder::register_data_producer(
    size_t data_index, GraphVertexBuilder* producer) noexcept {
  _producers_for_data_index[data_index].emplace(producer);
}
// GraphBuilder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// GraphVertexBuilder begin
GraphVertexBuilder& GraphVertexBuilder::set_name(StringView name) noexcept {
  _name = name;
  return *this;
}

GraphVertexBuilder& GraphVertexBuilder::name(StringView name) noexcept {
  return set_name(name);
}

GraphDependencyBuilder& GraphVertexBuilder::anonymous_depend() noexcept {
  auto index = _anonymous_dependencies.size();
  auto dependency = new GraphDependencyBuilder;
  dependency->set_source(*this);
  dependency->set_index(index);
  _anonymous_dependencies.emplace_back(dependency);
  return *dependency;
}

GraphDependencyBuilder& GraphVertexBuilder::named_depend(
    StringView name) noexcept {
  auto index = _named_dependencies.size();
  auto result =
      _dependency_index_by_name.emplace(::std::make_pair(name, index));
  if (result.second) {
    auto dependency = new GraphDependencyBuilder;
    dependency->set_source(*this);
    dependency->set_name(name);
    _named_dependencies.emplace_back(dependency);
    return *dependency;
  }
  return *_named_dependencies[result.first->second];
}

GraphEmitBuilder& GraphVertexBuilder::anonymous_emit() noexcept {
  auto index = _anonymous_emits.size();
  auto emit = new GraphEmitBuilder;
  emit->set_source(*this);
  emit->set_index(index);
  _anonymous_emits.emplace_back(emit);
  return *emit;
}

GraphEmitBuilder& GraphVertexBuilder::named_emit(StringView name) noexcept {
  auto index = _named_emits.size();
  auto result = _emit_index_by_name.emplace(::std::make_pair(name, index));
  if (result.second) {
    auto emit = new GraphEmitBuilder;
    emit->set_source(*this);
    emit->set_name(name);
    _named_emits.emplace_back(emit);
    return *emit;
  }
  return *_named_emits[result.first->second];
}

ssize_t GraphVertexBuilder::index_for_named_dependency(
    ::babylon::StringView name) const noexcept {
  auto iter = _dependency_index_by_name.find(name);
  if (ABSL_PREDICT_FALSE(iter == _dependency_index_by_name.end())) {
    return -1;
  }
  return static_cast<ssize_t>(iter->second);
}

ssize_t GraphVertexBuilder::index_for_named_emit(
    ::babylon::StringView name) const noexcept {
  auto iter = _emit_index_by_name.find(name);
  if (ABSL_PREDICT_FALSE(iter == _emit_index_by_name.end())) {
    return -1;
  }
  return static_cast<ssize_t>(iter->second);
}

void GraphVertexBuilder::set_graph(GraphBuilder& graph) noexcept {
  _graph = &graph;
}

void GraphVertexBuilder::set_index(size_t index) noexcept {
  _index = index;
}

GraphVertexBuilder& GraphVertexBuilder::set_allow_trivial(bool allow) noexcept {
  _allow_trivial = allow;
  return *this;
}

int GraphVertexBuilder::finish() noexcept {
  for_each_dependency([&](GraphDependencyBuilder& dependency) {
    dependency.finish();
  });
  for_each_emit([&](GraphEmitBuilder& emit) {
    emit.finish();
  });

  auto processor = _processor_creator();
  if (!processor) {
    BABYLON_LOG(WARNING) << "processor creater for " << *this << " not usable";
    return -1;
  }

  if (0 != processor->config(_raw_option, _option)) {
    BABYLON_LOG(WARNING) << "processor config for " << *this << " failed";
    return -1;
  }

  return 0;
}

int GraphVertexBuilder::build(Graph& graph,
                              GraphVertex& vertex) const noexcept {
  vertex.set_graph(graph);
  vertex.set_builder(*this);

  auto processor = _processor_creator();
  if (!processor) {
    BABYLON_LOG(WARNING) << *this << " build failed for no valid processor";
    return -1;
  }
  vertex.set_processor(::std::move(processor));

  auto& dependencies = vertex.dependencies();
  dependencies.resize(_named_dependencies.size() +
                      _anonymous_dependencies.size());

  size_t i = 0;
  for_each_dependency([&](const GraphDependencyBuilder& builder) {
    auto& dependency = dependencies[i++];
    builder.build(graph, vertex, dependency);
  });

  auto& emits = vertex.emits();
  emits.resize(_named_emits.size() + _anonymous_emits.size());
  i = 0;
  for_each_emit([&](const GraphEmitBuilder& builder) {
    auto& emit = emits[i++];
    builder.build(graph, vertex, emit);
  });

  auto ret = vertex.setup();
  if (ABSL_PREDICT_FALSE(ret != 0)) {
    BABYLON_LOG(WARNING) << "set up " << vertex << " failed";
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// GraphDependencyBuilder begin
GraphDependencyBuilder& GraphDependencyBuilder::to(StringView target) noexcept {
  _target = target;
  return *this;
}

GraphDependencyBuilder& GraphDependencyBuilder::on(
    StringView condition) noexcept {
  _condition = condition;
  _establish_value = true;
  return *this;
}

GraphDependencyBuilder& GraphDependencyBuilder::unless(
    StringView condition) noexcept {
  _condition = condition;
  _establish_value = false;
  return *this;
}

void GraphDependencyBuilder::set_source(GraphVertexBuilder& source) noexcept {
  _source = &source;
}

void GraphDependencyBuilder::set_name(StringView name) noexcept {
  _name = name;
}

void GraphDependencyBuilder::set_index(size_t index) noexcept {
  _index = index;
}

void GraphDependencyBuilder::finish() noexcept {
  _target_index = _source->graph().get_or_allocate_data_index(_target);
  if (!_condition.empty()) {
    _condition_index = _source->graph().get_or_allocate_data_index(_condition);
  }
}

void GraphDependencyBuilder::build(Graph& graph, GraphVertex& vertex,
                                   GraphDependency& dependency) const noexcept {
  auto& target = graph.data()[_target_index];
  target.add_successor(dependency);
  dependency.source(vertex);
  dependency.target(target);

  if (!_condition.empty()) {
    auto& condition = graph.data()[_condition_index];
    condition.add_successor(dependency);
    dependency.condition(condition, _establish_value);
  }
}
// GraphDependencyBuilder end
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GraphEmitBuilder begin
GraphEmitBuilder& GraphEmitBuilder::to(StringView target) noexcept {
  _target = target;
  return *this;
}

void GraphEmitBuilder::set_source(GraphVertexBuilder& source) noexcept {
  _source = &source;
}

void GraphEmitBuilder::set_name(StringView name) noexcept {
  _name = name;
}

void GraphEmitBuilder::set_index(size_t index) noexcept {
  _index = index;
}

void GraphEmitBuilder::finish() noexcept {
  _target_index = _source->graph().get_or_allocate_data_index(_target);
  _source->graph().register_data_producer(_target_index, _source);
}

void GraphEmitBuilder::build(Graph& graph, GraphVertex& vertex,
                             GraphData*& emit) const noexcept {
  auto& data = graph.data()[_target_index];
  data.producer(vertex);
  emit = &data;
}
// GraphEmitBuilder end
///////////////////////////////////////////////////////////////////////////////

} // namespace anyflow
BABYLON_NAMESPACE_END
