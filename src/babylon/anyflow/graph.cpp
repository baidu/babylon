#include "babylon/anyflow/graph.h"

#include "babylon/anyflow/data.h"
#include "babylon/anyflow/executor.h"
#include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

GraphData* Graph::find_data(StringView name) noexcept {
    auto it = _data_for_name.find(name);
    if (ABSL_PREDICT_FALSE(it == _data_for_name.end())) {
        BABYLON_LOG(WARNING) << "no data named " << name << " in graph";
        return nullptr;
    }
    return it->second;
}

Closure Graph::run(GraphData* data[], size_t size) noexcept {
    auto closure = _executor->create_closure();
    auto context = closure.context();
    context->all_data_num(_data.size());

    ::absl::InlinedVector<GraphVertex*, 128> runnable_vertexes;
    for (size_t i = 0; i < size; ++i) {
        if (ABSL_PREDICT_FALSE(!data[i]->bind(*context))) {
            continue;
        }
        if (0 != data[i]->recursive_activate(runnable_vertexes, context)) {
            BABYLON_LOG(WARNING) << "activate " << *data[i] << " failed";
            context->finish(-1);
            context->fire();
            return closure;
        }
    }

    // lijiang01 todo: 区分是否是trivial算子，先启动其他算子的异步执行
    // 最后再串行执行这些trivial算子
    while (!runnable_vertexes.empty()) {
        auto vertex = runnable_vertexes.back();
        runnable_vertexes.pop_back();
        vertex->invoke(runnable_vertexes);
    }
    context->fire();
    return closure;
}

void Graph::reset() noexcept {
    for (auto& one_data : _data) {
        one_data.reset();
    }
    for (auto& vertex : _vertexes) {
        vertex.reset();
    }
    _memory_resource.release();
    _reusable_manager.clear();
}

int Graph::func_each_vertex(std::function<int(GraphVertex&)> func) noexcept {
    for (auto& vertex : _vertexes) {
        if (func(vertex) != 0) {
            BABYLON_LOG(WARNING) << "func on " << vertex << " failed";
            return -1; 
        }
    } 

    return 0;
}

void Graph::initialize_data(const IndexForNameMap& index_for_name) noexcept {
    _data_for_name.clear();
    _data.~vector();
    new (&_data) ::std::vector<GraphData> {index_for_name.size()};
    for (const auto& pair : index_for_name) {
        auto& data = _data[pair.second];
        data.set_name(pair.first);
        data.set_graph(*this);
        _data_for_name.emplace(pair.first, &data);
    }
}

void Graph::initialize_vertexes(size_t vertex_num) noexcept {
    _vertexes.~vector();
    new (&_vertexes) ::std::vector<GraphVertex> {vertex_num};
}

::std::vector<GraphData>& Graph::data() noexcept {
    return _data;
}

::std::vector<GraphVertex>& Graph::vertexes() noexcept {
    return _vertexes;
}

void Graph::set_executor(GraphExecutor& executor) noexcept {
    _executor = &executor;
}

void Graph::set_page_allocator(::babylon::PageAllocator& allocator) noexcept {
    _memory_resource = SwissMemoryResource {allocator};
}

} // anyflow
BABYLON_NAMESPACE_END
