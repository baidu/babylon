#pragma once

#include BABYLON_EXTERNAL(absl/container/flat_hash_map.h)   // absl::flat_hash_map
#include "babylon/string_view"              // babylon::StringView

#include <vector>                           // std::vector

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

using IndexMap = ::absl::flat_hash_map<::std::string, size_t,
                                       ::std::hash<::babylon::StringView>,
                                       ::std::equal_to<::babylon::StringView>>;

class GraphProcessor;
class ProcessorDescriptor {
public:
private:
    ::std::function<::std::unique_ptr<GraphProcessor>()> _processor_creator;
};

class VertexDescriptor {
public:
private:
    ::std::string _name;
    size_t _index {0};
    ::std::function<::std::unique_ptr<GraphProcessor>()> _processor_creator;
    ::babylon::Any _raw_option;
    ::babylon::Any _option;
    bool _allow_trivial {true};
    
    ::std::vector<DependencyDescriptor> _named_dependencies;
    IndexMap _dependency_index_for_name;

    ::std::vector<DependencyDescriptor> _anonymous_dependencies;
    IndexMap _dependency_index_for_name;
    ::std::vector<::std::unique_ptr<GraphDependencyBuilder>> _named_dependencies;
    ::std::vector<::std::unique_ptr<GraphDependencyBuilder>> _anonymous_dependencies;

    // 输出关系
    Map _emit_index_by_name;
    ::std::vector<::std::unique_ptr<GraphEmitBuilder>> _named_emits;
    ::std::vector<::std::unique_ptr<GraphEmitBuilder>> _anonymous_emits;
};

class GraphDescriptor {
public:

private:
    using IndexMap = ::absl::flat_hash_map<::std::string, size_t,
                                      ::std::hash<::babylon::StringView>,
                                      ::std::equal_to<::babylon::StringView>>;

    ::std::string _name;

    ::std::vector<VertexDescriptor> _vertexes;

    ::std::vector<DataDescriptor> _data;
    ::absl::flat_hash_map<::std::string, size_t,
                          ::std::hash<::babylon::StringView>,
                          ::std::equal_to<::babylon::StringView>> _data_index_for_name;
};

} // anyflow
BABYLON_NAMESPACE_END

#include "babylon/anyflow/graph.hpp"
