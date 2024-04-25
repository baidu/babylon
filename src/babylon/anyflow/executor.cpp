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
    static InplaceGraphExecutor instance;
    return instance;
}
////////////////////////////////////////////////////////////////////////////////

int ThreadPoolGraphExecutor::initialize(size_t worker_num,
                                        size_t queue_capacity) noexcept {
    return _executor.initialize(worker_num, queue_capacity);
}

void ThreadPoolGraphExecutor::stop() noexcept {
    _executor.stop();
}

Closure ThreadPoolGraphExecutor::create_closure() noexcept {
    return Closure::create<SchedInterface>(*this);
}

int ThreadPoolGraphExecutor::run(GraphVertex* vertex,
                                 GraphVertexClosure&& closure) noexcept {
    _executor.submit([closure = ::std::move(closure), vertex] () mutable {
        vertex->run(::std::move(closure));
    });
    return 0;
}

int ThreadPoolGraphExecutor::run(ClosureContext* closure,
                                 Closure::Callback* callback) noexcept {
    _executor.submit([=] {
        closure->run(callback);
    });
    return 0;
}

} // anyflow
BABYLON_NAMESPACE_END
