#pragma once

#include "babylon/anyflow/builder.h"
#include "babylon/application_context.h"
#include "babylon/concurrent/object_pool.h"

#include "bthread_graph_executor.h"
#include "butil/logging.h"
#include "yaml-cpp/yaml.h"

class GraphConfigurator {
 public:
  using Graph = ::babylon::anyflow::Graph;
  using GraphProcessor = ::babylon::anyflow::GraphProcessor;
  using ApplicationContext = ::babylon::ApplicationContext;

  int load(const ::std::string& configration_file) {
    //////////////////////////////////////////////////////////////////
    // Build graph by configuration with yaml
    auto node = ::YAML::LoadFile(configration_file);
    for (auto vertex_node : node["vertexes"]) {
      auto& name = vertex_node["name"].Scalar();
      auto accessor =
          ApplicationContext::instance().component_accessor<GraphProcessor>(
              name);
      if (!accessor) {
        LOG(WARNING) << "find component " << name << " failed";
        return -1;
      }
      auto option = vertex_node["option"];
      auto& vertex = _builder.add_vertex([accessor, option]() mutable {
        ::babylon::Any option_any {option};
        return ::std::unique_ptr<GraphProcessor> {
            accessor.create(option_any).release()};
      });
      if (vertex_node["depends"].IsMap()) {
        for (auto depend_node : vertex_node["depends"]) {
          auto& parameter = depend_node.first.Scalar();
          auto& argument = depend_node.second.Scalar();
          vertex.named_depend(parameter).to(argument);
        }
      } else {
        for (auto argument_node : vertex_node["depends"]) {
          vertex.anonymous_depend().to(argument_node.Scalar());
        }
      }
      for (auto depend_node : vertex_node["emits"]) {
        auto& parameter = depend_node.first.Scalar();
        auto& argument = depend_node.second.Scalar();
        vertex.named_emit(parameter).to(argument);
      }
    }
    //////////////////////////////////////////////////////////////////

    // Use bthread to run graph processor
    _builder.set_executor(::babylon::anyflow::BthreadGraphExecutor::instance());

    if (0 != _builder.finish()) {
      LOG(WARNING) << "finish graph builder failed";
      return -1;
    }

    // Cache graph instance
    {
      auto cache_size = node["cache_size"].as<size_t>();
      _graph_pool.reserve_and_clear(cache_size);
      _graph_pool.set_creator([this] {
        return _builder.build();
      });
      _graph_pool.set_recycler([](Graph& graph) {
        graph.reset();
      });
    }

    return 0;
  }

  ::std::unique_ptr<Graph, ::babylon::ObjectPool<Graph>::Deleter> get_graph() {
    return _graph_pool.pop();
  }

 private:
  ::babylon::anyflow::GraphBuilder _builder;
  ::babylon::ObjectPool<Graph> _graph_pool;
};
