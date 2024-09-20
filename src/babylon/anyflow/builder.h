#pragma once

#include "babylon/any.h"
#include "babylon/anyflow/dependency.h"
#include "babylon/anyflow/executor.h"
#include "babylon/reusable/memory_resource.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/container/flat_hash_map.h)
// clang-format on

#include <memory>
#include <vector>

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

class Graph;
class GraphExecutor;
class GraphProcessor;
class GraphVertexBuilder;
class GraphBuilder {
 private:
  using PageAllocator = ::babylon::PageAllocator;
  using StringView = ::babylon::StringView;
  using SystemPageAllocator = ::babylon::SystemPageAllocator;

 public:
  GraphBuilder() = default;
  GraphBuilder(GraphBuilder&&) = delete;
  GraphBuilder(const GraphBuilder&) = delete;
  GraphBuilder& operator=(GraphBuilder&&) = delete;
  GraphBuilder& operator=(const GraphBuilder&) = delete;
  ~GraphBuilder() noexcept = default;

  // 设置名字，用于日志打印
  GraphBuilder& set_name(StringView name) noexcept;
  inline StringView name() const noexcept;

  // 设置executor，用于支持实际图运行
  GraphBuilder& set_executor(GraphExecutor& executor) noexcept;
  inline GraphExecutor& executor() const noexcept;

  // 设置PageHeap，用于算子内分配内存
  GraphBuilder& set_page_allocator(PageAllocator& allocator) noexcept;
  inline PageAllocator& page_allocator() const noexcept;

  // 加入一个processor，返回GraphVertexBuilder进行进一步依赖设置
  // processor支持直接设置实例或者使用名字从context中组装实例
  template <typename C>
  inline GraphVertexBuilder& add_vertex(C&& creator) noexcept;

  // 遍历所有节点，做一些额外的操作
  // 比如提取其中的表达式依赖，进一步处理
  template <typename C>
  inline void for_each_vertex(C&& callback) noexcept;

  // 完成构建，检测整体正确性
  // 并将各个builder的string描述转为序号加速
  int finish() noexcept;

  // 之后可以反复通过build获取Graph实例
  ::std::unique_ptr<Graph> build() const noexcept;

 private:
  size_t get_or_allocate_data_index(StringView name) noexcept;
  void register_data_producer(size_t data_index,
                              GraphVertexBuilder* producer) noexcept;

  // 自身信息
  ::std::string _name;
  GraphExecutor* _executor {&InplaceGraphExecutor::instance()};
  PageAllocator* _page_allocator {&SystemPageAllocator::instance()};

  // 图内节点
  ::std::vector<::std::unique_ptr<GraphVertexBuilder>> _vertexes;

  // 构造图使用的符号表
  ::std::unordered_map<::std::string, size_t> _data_index_for_name;
  ::std::unordered_map<size_t, std::unordered_set<GraphVertexBuilder*>>
      _producers_for_data_index;

  friend class GraphDependencyBuilder;
  friend class GraphEmitBuilder;
};

class GraphData;
class GraphVertex;
class GraphDependency;
class GraphEmitBuilder;
class GraphDependencyBuilder;
class GraphVertexBuilder {
 private:
  using StringView = ::babylon::StringView;

 public:
  GraphVertexBuilder() = default;
  GraphVertexBuilder(GraphVertexBuilder&&) = delete;
  GraphVertexBuilder(const GraphVertexBuilder&) = delete;
  GraphVertexBuilder& operator=(GraphVertexBuilder&&) = delete;
  GraphVertexBuilder& operator=(const GraphVertexBuilder&) = delete;
  ~GraphVertexBuilder() noexcept = default;

  // 获取所属GraphBuilder
  inline GraphBuilder& graph() noexcept;
  inline const GraphBuilder& graph() const noexcept;

  // 获取节点序号，用于日志打印
  inline size_t index() const noexcept;

  // 设置算子名称，用于日志打印
  GraphVertexBuilder& set_name(StringView name) noexcept;
  GraphVertexBuilder& name(StringView name) noexcept;
  inline StringView name() const noexcept;

  // 添加一个命名或匿名依赖
  GraphDependencyBuilder& anonymous_depend() noexcept;
  GraphDependencyBuilder& named_depend(StringView name) noexcept;

  // 添加一个命名或匿名输出
  GraphEmitBuilder& anonymous_emit() noexcept;
  GraphEmitBuilder& named_emit(StringView name) noexcept;

  // 设置option，用于定制算子在vertex的行为
  // 最终被算子在setup阶段使用
  template <typename T>
  inline GraphVertexBuilder& option(T&& option) noexcept;
  template <typename T>
  inline const T* option() const noexcept;

  // 遍历所有依赖和输出，做一些额外的操作
  // 比如提取其中的表达式依赖，进一步处理
  template <typename C>
  inline void for_each_dependency(C&& callback) noexcept;
  template <typename C>
  inline void for_each_dependency(C&& callback) const noexcept;
  template <typename C>
  inline void for_each_emit(C&& callback) noexcept;
  template <typename C>
  inline void for_each_emit(C&& callback) const noexcept;

  ssize_t index_for_named_dependency(::babylon::StringView name) const noexcept;
  ssize_t index_for_named_emit(::babylon::StringView name) const noexcept;

 private:
  void set_graph(GraphBuilder& graph) noexcept;
  void set_index(size_t index) noexcept;
  template <typename C>
  inline void set_processor_creator(C&& creator) noexcept;

  GraphVertexBuilder& set_allow_trivial(bool allow) noexcept;
  inline bool allow_trivial() const noexcept;

  int finish() noexcept;
  int build(Graph& graph, GraphVertex& vertex) const noexcept;

  inline GraphDependency* named_dependency(
      const ::std::string& name,
      ::std::vector<GraphDependency>& dependencies) const noexcept;
  inline GraphDependency* anonymous_dependency(
      size_t index,
      ::std::vector<GraphDependency>& dependencies) const noexcept;
  inline size_t anonymous_dependency_size() const noexcept;
  inline GraphData* named_emit(const ::std::string& name,
                               ::std::vector<GraphData*>& data) const noexcept;
  inline GraphData* anonymous_emit(
      size_t index, ::std::vector<GraphData*>& data) const noexcept;
  inline size_t anonymous_emit_size() const noexcept;

  // 自身信息
  GraphBuilder* _graph {nullptr};
  ::std::string _name;
  size_t _index {0};
  ::std::function<::std::unique_ptr<GraphProcessor>()> _processor_creator;
  ::babylon::Any _raw_option;
  ::babylon::Any _option;
  bool _allow_trivial {true};

  using Map = ::absl::flat_hash_map<::std::string, size_t,
                                    ::std::hash<::babylon::StringView>,
                                    ::std::equal_to<::babylon::StringView>>;
  // 依赖关系
  Map _dependency_index_by_name;
  ::std::vector<::std::unique_ptr<GraphDependencyBuilder>> _named_dependencies;
  ::std::vector<::std::unique_ptr<GraphDependencyBuilder>>
      _anonymous_dependencies;

  // 输出关系
  Map _emit_index_by_name;
  ::std::vector<::std::unique_ptr<GraphEmitBuilder>> _named_emits;
  ::std::vector<::std::unique_ptr<GraphEmitBuilder>> _anonymous_emits;

  friend class GraphVertex;
  friend class GraphBuilder;
};

class GraphDependency;
class GraphDependencyBuilder {
 private:
  using StringView = ::babylon::StringView;

 public:
  GraphDependencyBuilder() = default;
  GraphDependencyBuilder(GraphDependencyBuilder&&) noexcept = default;
  GraphDependencyBuilder(const GraphDependencyBuilder&) = delete;
  GraphDependencyBuilder& operator=(GraphDependencyBuilder&&) noexcept =
      default;
  GraphDependencyBuilder& operator=(const GraphDependencyBuilder&) = delete;
  ~GraphDependencyBuilder() noexcept = default;

  inline const ::std::string& name() const noexcept;
  inline size_t index() const noexcept;
  inline const ::std::string& target() const noexcept;
  inline const ::std::string& condition() const noexcept;

  // 设置依赖目标GraphData名
  GraphDependencyBuilder& to(StringView target) noexcept;
  // 设置当名为condition的GraphData为true时成立
  GraphDependencyBuilder& on(StringView condition) noexcept;
  // 设置当名为condition的GraphData为false时成立
  GraphDependencyBuilder& unless(StringView condition) noexcept;

 private:
  void set_source(GraphVertexBuilder& source) noexcept;
  void set_name(StringView name) noexcept;
  void set_index(size_t index) noexcept;

  void finish() noexcept;
  void build(Graph& graph, GraphVertex& vertex,
             GraphDependency& dependency) const noexcept;

  // 描述
  GraphVertexBuilder* _source;
  ::std::string _name;
  size_t _index {0};
  ::std::string _target;
  ::std::string _condition;
  bool _establish_value {false};

  // 序号表
  size_t _target_index {0};
  size_t _condition_index {0};

  friend class GraphBuilder;
  friend class GraphVertexBuilder;
};

class GraphEmitBuilder {
 private:
  using StringView = ::babylon::StringView;

 public:
  GraphEmitBuilder() = default;
  GraphEmitBuilder(GraphEmitBuilder&&) noexcept = default;
  GraphEmitBuilder(const GraphEmitBuilder&) = delete;
  GraphEmitBuilder& operator=(GraphEmitBuilder&&) noexcept = default;
  GraphEmitBuilder& operator=(const GraphEmitBuilder&) = delete;
  ~GraphEmitBuilder() noexcept = default;

  // 设置输出目标GraphData名
  GraphEmitBuilder& to(StringView target) noexcept;

  inline const ::std::string& name() const noexcept;
  inline size_t index() const noexcept;
  inline const ::std::string& target() const noexcept;

 private:
  void set_source(GraphVertexBuilder& source) noexcept;
  void set_name(StringView name) noexcept;
  void set_index(size_t index) noexcept;

  void finish() noexcept;
  void build(Graph& graph, GraphVertex& vertex,
             GraphData*& emit) const noexcept;

  // 描述
  GraphVertexBuilder* _source;
  ::std::string _name;
  size_t _index {0};
  ::std::string _target;

  // 序号表
  size_t _target_index {0};

  friend class GraphVertexBuilder;
};

} // namespace anyflow
BABYLON_NAMESPACE_END

#include "babylon/anyflow/builder.hpp"
