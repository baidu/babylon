#pragma once

#include "babylon/anyflow/vertex.h"
#include "babylon/anyflow/builder.h"
#include "babylon/anyflow/closure.h"
#include "babylon/anyflow/dependency.h"
#include "babylon/anyflow/graph.h"
#include "babylon/anyflow/data.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

////////////////////////////////////////////////////////////////////////////////
// GraphProcessor begin
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline GraphVertex& GraphProcessor::vertex() noexcept {
    return *_vertex;
}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline const T* GraphProcessor::option() noexcept {
    return _vertex->option<T>();
}

template <typename T, typename... Args>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline T* GraphProcessor::create_object(Args&&... args) noexcept {
    return _vertex->graph().create_object<T>(::std::forward<Args>(args)...);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline ::babylon::SwissMemoryResource&
GraphProcessor::memory_resource() noexcept {
    return _vertex->graph().memory_resource();
}

template <typename T, typename... Args>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline ::babylon::ReusableAccessor<T>
GraphProcessor::create_reusable_object(Args&&... args) noexcept {
    return _vertex->graph().create_reusable_object<T>(
            ::std::forward<Args>(args)...);
}
// GraphProcessor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// GraphVertexClosure begin
inline GraphVertexClosure::GraphVertexClosure(
        ClosureContext& closure,
        GraphVertex& vertex) noexcept :
            _closure(&closure),
            _vertex(&vertex) {
    _closure->depend_vertex_add();
}

inline GraphVertexClosure::GraphVertexClosure(
        GraphVertexClosure&& other) noexcept :
            _closure(other._closure),
            _vertex(other._vertex) {
    other._closure = nullptr;
    other._vertex = nullptr;
}

inline GraphVertexClosure& GraphVertexClosure::operator=(
        GraphVertexClosure&& other) noexcept {
    ::std::swap(_closure, other._closure);
    ::std::swap(_vertex, other._vertex);
    return *this;
}

inline GraphVertexClosure::~GraphVertexClosure() noexcept {
    done();
}

inline void GraphVertexClosure::done() noexcept {
    done(0);
}

inline void GraphVertexClosure::done(int error_code) noexcept {
    if (_closure != nullptr) {
        if (error_code != 0) {
            BABYLON_LOG(WARNING) << *_vertex << " done with " << error_code;
            _closure->finish(error_code);
        } else {
            _vertex->flush_emits();
        }
        _closure->depend_vertex_sub();
        _closure = nullptr;
        _vertex = nullptr;
    }
}

inline bool GraphVertexClosure::finished() const noexcept {
    return _closure->finished();
}
// GraphVertexClosure end
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// GraphVertex begin
inline Graph& GraphVertex::graph() noexcept {
    return *_graph;
}

inline ::babylon::StringView GraphVertex::name() const noexcept {
    return _builder->name();
}

inline size_t GraphVertex::index() const noexcept {
    return _builder->index();
}

inline GraphDependency* GraphVertex::named_dependency(size_t index) noexcept {
    assert(index <= _dependencies.size());
    return &_dependencies[index];
}

inline GraphDependency* GraphVertex::anonymous_dependency(
        size_t index) noexcept {
    return _builder->anonymous_dependency(index, _dependencies);
}

inline size_t GraphVertex::anonymous_dependency_size() const noexcept {
    return _builder->anonymous_dependency_size();
}

inline GraphData* GraphVertex::named_emit(size_t index) noexcept {
    assert(index <= _emits.size());
    return _emits[index];
}

inline GraphData* GraphVertex::anonymous_emit(size_t index) noexcept {
    return _builder->anonymous_emit(index, _emits);
}

inline size_t GraphVertex::anonymous_emit_size() const noexcept {
    return _builder->anonymous_emit_size();
}

template <typename T>
inline const T* GraphVertex::option() const noexcept {
    return _builder->option<T>();
}

template <typename T>
inline const T* GraphVertex::get_graph_context() const noexcept {
    return _graph->context<T>();
}

inline bool GraphVertex::ready(GraphDependency*) noexcept {
    return _waiting_num.fetch_sub(1, ::std::memory_order_acq_rel) == 1;
}

inline ClosureContext* GraphVertex::closure() noexcept {
    return _closure;
}

inline GraphVertex::VertexStack* GraphVertex::runnable_vertexes() noexcept {
    return _runnable_vertexes;
}

inline void GraphVertex::run(GraphVertexClosure&& closure) noexcept {
    if (ABSL_PREDICT_FALSE(closure.finished())) {
        closure.done(0);
        return;
    }
    _processor->process(*this, ::std::move(closure));
}

inline ::std::ostream& operator<<(::std::ostream& os,
                                  const GraphVertex& vertex) noexcept {
    os << "vertex[" << vertex.name() << "][" << vertex.index() << "]";
    return os;
}

// GraphVertex end
////////////////////////////////////////////////////////////////////////////////

} // anyflow
BABYLON_NAMESPACE_END
