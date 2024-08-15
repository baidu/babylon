#pragma once

#include "babylon/anyflow/builder.h"
#include "babylon/anyflow/graph.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

///////////////////////////////////////////////////////////////////////////////
// GraphBuilder begin
inline ::babylon::StringView GraphBuilder::name() const noexcept {
  return _name;
}

inline GraphExecutor& GraphBuilder::executor() const noexcept {
  return *_executor;
}

template <typename C>
inline GraphVertexBuilder& GraphBuilder::add_vertex(C&& creator) noexcept {
  auto vertex = new GraphVertexBuilder;
  vertex->set_graph(*this);
  vertex->set_index(_vertexes.size());
  vertex->set_processor_creator(::std::forward<C>(creator));
  _vertexes.emplace_back(vertex);
  return *vertex;
}

template <typename C>
inline void GraphBuilder::for_each_vertex(C&& callback) noexcept {
  for (auto& vertex : _vertexes) {
    callback(*vertex);
  }
}

inline ::std::ostream& operator<<(::std::ostream& os,
                                  const GraphBuilder& builder) noexcept {
  os << "graph[";
  if (!builder.name().empty()) {
    os << builder.name();
  } else {
    os << &builder;
  }
  return os << "]";
}
// GraphBuilder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GraphVertexBuilder begin
inline size_t GraphVertexBuilder::index() const noexcept {
  return _index;
}

inline ::babylon::StringView GraphVertexBuilder::name() const noexcept {
  return _name;
}

inline GraphBuilder& GraphVertexBuilder::graph() noexcept {
  return *_graph;
}

inline const GraphBuilder& GraphVertexBuilder::graph() const noexcept {
  return *_graph;
}

template <typename C>
inline void GraphVertexBuilder::for_each_dependency(C&& callback) noexcept {
  for (auto& dependency : _named_dependencies) {
    callback(*dependency);
  }
  for (auto& dependency : _anonymous_dependencies) {
    callback(*dependency);
  }
}

template <typename C>
inline void GraphVertexBuilder::for_each_dependency(
    C&& callback) const noexcept {
  for (auto& dependency : _named_dependencies) {
    callback(const_cast<const GraphDependencyBuilder&>(*dependency));
  }
  for (auto& dependency : _anonymous_dependencies) {
    callback(const_cast<const GraphDependencyBuilder&>(*dependency));
  }
}

template <typename C>
inline void GraphVertexBuilder::for_each_emit(C&& callback) noexcept {
  for (auto& emit : _named_emits) {
    callback(*emit);
  }
  for (auto& emit : _anonymous_emits) {
    callback(*emit);
  }
}

template <typename C>
inline void GraphVertexBuilder::for_each_emit(C&& callback) const noexcept {
  for (auto& emit : _named_emits) {
    callback(const_cast<const GraphEmitBuilder&>(*emit));
  }
  for (auto& emit : _anonymous_emits) {
    callback(const_cast<const GraphEmitBuilder&>(*emit));
  }
}

template <typename C>
inline void GraphVertexBuilder::set_processor_creator(C&& creator) noexcept {
  _processor_creator = ::std::forward<C>(creator);
}

template <typename T>
GraphVertexBuilder& GraphVertexBuilder::option(T&& option) noexcept {
  _raw_option = ::std::move(option);
  return *this;
}

template <>
inline const ::babylon::Any* GraphVertexBuilder::option<::babylon::Any>()
    const noexcept {
  return &_option;
}

template <typename T>
const T* GraphVertexBuilder::option() const noexcept {
  return _option.get<T>();
}

inline bool GraphVertexBuilder::allow_trivial() const noexcept {
  return _allow_trivial;
}

GraphDependency* GraphVertexBuilder::named_dependency(
    const ::std::string& name,
    ::std::vector<GraphDependency>& dependencies) const noexcept {
  auto it = _dependency_index_by_name.find(name);
  if (ABSL_PREDICT_TRUE(it != _dependency_index_by_name.end())) {
    return &dependencies[it->second];
  }
  return nullptr;
}

GraphDependency* GraphVertexBuilder::anonymous_dependency(
    size_t index, ::std::vector<GraphDependency>& dependencies) const noexcept {
  auto real_index = _named_dependencies.size() + index;
  if (ABSL_PREDICT_TRUE(real_index < dependencies.size())) {
    return &dependencies[real_index];
  }
  return nullptr;
}

size_t GraphVertexBuilder::anonymous_dependency_size() const noexcept {
  return _anonymous_dependencies.size();
}

GraphData* GraphVertexBuilder::named_emit(
    const ::std::string& name,
    ::std::vector<GraphData*>& emits) const noexcept {
  auto it = _emit_index_by_name.find(name);
  if (ABSL_PREDICT_TRUE(it != _emit_index_by_name.end())) {
    return emits[it->second];
  }
  return nullptr;
}

GraphData* GraphVertexBuilder::anonymous_emit(
    size_t index, ::std::vector<GraphData*>& data) const noexcept {
  auto real_index = _named_emits.size() + index;
  if (ABSL_PREDICT_TRUE(real_index < data.size())) {
    return data[real_index];
  }
  return nullptr;
}

size_t GraphVertexBuilder::anonymous_emit_size() const noexcept {
  return _anonymous_emits.size();
}

inline ::std::ostream& operator<<(::std::ostream& os,
                                  const GraphVertexBuilder& vertex) noexcept {
  os << "vertex[" << vertex.name() << "][" << vertex.index() << "]";
  return os;
}
// GraphVertexBuilder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GraphEmitBuilder begin
inline const ::std::string& GraphEmitBuilder::name() const noexcept {
  return _name;
}

inline size_t GraphEmitBuilder::index() const noexcept {
  return _index;
}

inline const ::std::string& GraphEmitBuilder::target() const noexcept {
  return _target;
}

inline ::std::ostream& operator<<(::std::ostream& os,
                                  const GraphEmitBuilder& emit) noexcept {
  os << "dependency[";
  if (emit.name().empty()) {
    os << emit.index();
  } else {
    os << emit.name();
  }
  return os << "]";
}
// GraphEmitBuilder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GraphDependencyBuilder begin
inline const ::std::string& GraphDependencyBuilder::name() const noexcept {
  return _name;
}

inline size_t GraphDependencyBuilder::index() const noexcept {
  return _index;
}

inline const ::std::string& GraphDependencyBuilder::target() const noexcept {
  return _target;
}

inline const ::std::string& GraphDependencyBuilder::condition() const noexcept {
  return _condition;
}

inline ::std::ostream& operator<<(
    ::std::ostream& os, const GraphDependencyBuilder& dependency) noexcept {
  os << "dependency[";
  if (dependency.name().empty()) {
    os << dependency.index();
  } else {
    os << dependency.name();
  }
  return os << "]";
}
// GraphDependencyBuilder end
///////////////////////////////////////////////////////////////////////////////

} // namespace anyflow
BABYLON_NAMESPACE_END
