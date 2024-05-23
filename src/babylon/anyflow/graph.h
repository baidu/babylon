#pragma once

#include "babylon/any.h"              // babylon::Any
#include "babylon/anyflow/closure.h"  // Closure
#include "babylon/reusable/manager.h" // babylon::SwissManager

// clang-format off
#include BABYLON_EXTERNAL(absl/container/flat_hash_map.h)  // absl::flat_hash_map
#include BABYLON_EXTERNAL(absl/container/inlined_vector.h) // absl::InlinedVector
// clang-format on

#include <vector> // std::vector

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

class GraphLog;
class GraphData;
class GraphVertex;
class GraphExecutor;
class Graph {
 private:
  template <typename T>
  using ReusableAccessor = ::babylon::ReusableAccessor<T>;
  using StringView = ::babylon::StringView;
  using SwissManager = ::babylon::SwissManager;
  using SwissMemoryResource = ::babylon::SwissMemoryResource;

 public:
  // 默认构造仅供GraphBuilder使用
  // 使用者应该通过GraphBuilder来构造图实例
  Graph() = default;
  // 禁用拷贝和移动
  Graph(Graph&&) = delete;
  Graph(const Graph&) = delete;
  Graph& operator=(Graph&&) = delete;
  Graph& operator=(const Graph&) = delete;
  ~Graph() noexcept = default;

  // 通过name找到对应的GraphData
  // 用于初始值的赋予，或者进一步通过run来发起一次求值运行
  GraphData* find_data(StringView name) noexcept;

  // 以指定的一系列GraphData为目的开始求值运行
  // 运行会从传入的根节点开始，按照DAG方式找到第一批无依赖叶节点
  // 并通过GraphExecutor来发起执行，并在完成后激活后续节点
  // 直到根节点完成
  //
  // 返回的Closure用于获取运行状态以及执行进行相关同步动作
  template <typename... D>
  inline Closure run(D... data) noexcept;
  Closure run(GraphData* data[], size_t size) noexcept;

  // 清理执行状态，以及中间产出的数据
  // 清理后重新使用进入下一轮run
  void reset() noexcept;

  // 获取Graph级别的共享上下文环境，类型可以自行设定
  // 主要用于在整个执行环境中共享一些基础常量环境
  // 典型例如log id / frame id等信息
  template <typename T>
  inline T* context() noexcept;

  // 获取Graph级别内存池，或直接使用内存池创建实例
  // 内存池在每次reset时会被清理，返回的实例也不再可用
  template <typename T, typename... Args>
  inline T* create_object(Args&&... args) noexcept;
  inline SwissMemoryResource& memory_resource() noexcept;

  // 创建Graph级别可重用实例
  // 需要T满足babylon::ReusableTraits::REUSABLE特性
  // 返回的实例会在每次reset时被逻辑清空
  template <typename T, typename... Args>
  inline ReusableAccessor<T> create_reusable_object(Args&&... args) noexcept;

  // 对Graph中每个GraphVertex执行一次func调用
  // 用于实现一些高级操作，例如支持实现trace延时收集等机制
  int func_each_vertex(std::function<int(GraphVertex&)> func) noexcept;

 private:
  using VertexStack = ::absl::InlinedVector<GraphVertex*, 128>;
  using IndexForNameMap = ::std::unordered_map<::std::string, size_t>;

  // 结构修改函数仅供GraphBuilder使用
  void initialize_data(const IndexForNameMap& index_for_name) noexcept;
  void initialize_vertexes(size_t vertex_num) noexcept;
  ::std::vector<GraphData>& data() noexcept;
  ::std::vector<GraphVertex>& vertexes() noexcept;
  void set_executor(GraphExecutor& executor) noexcept;
  void set_page_allocator(::babylon::PageAllocator& allocator) noexcept;

  GraphExecutor* _executor {nullptr};
  ::std::vector<GraphVertex> _vertexes;
  ::std::vector<GraphData> _data;
  ::absl::flat_hash_map<::std::string, GraphData*, ::std::hash<StringView>,
                        ::std::equal_to<StringView>>
      _data_for_name;
  ::babylon::Any _context;
  SwissMemoryResource _memory_resource;
  SwissManager _reusable_manager;

  friend class GraphBuilder;
  friend class GraphData;
  friend class GraphDependencyBuilder;
  friend class GraphEmitBuilder;
  friend class GraphLog;
};

} // namespace anyflow
BABYLON_NAMESPACE_END

#include "babylon/anyflow/graph.hpp"
