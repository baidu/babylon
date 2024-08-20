#pragma once

#include "babylon/any.h"
#include "babylon/reusable/manager.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/container/inlined_vector.h)
// clang-format on

#include "boost/preprocessor/cat.hpp"
#include "boost/preprocessor/comparison/equal.hpp"
#include "boost/preprocessor/comparison/greater.hpp"
#include "boost/preprocessor/comparison/less.hpp"
#include "boost/preprocessor/control/if.hpp"
#include "boost/preprocessor/punctuation/remove_parens.hpp"
#include "boost/preprocessor/seq/for_each.hpp"
#include "boost/preprocessor/tuple/push_back.hpp"
#include "boost/preprocessor/tuple/push_front.hpp"
#include "boost/preprocessor/tuple/size.hpp"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

// 算子基类
// 每个GraphVertex实例都会绑定一个不同的GraphProcessor
// 可以使用成员变量来放置一些内部计算结果和计算缓冲区
class GraphVertex;
class ClosureContext;
class GraphVertexClosure;
class GraphProcessor {
 public:
  virtual ~GraphProcessor() noexcept;

  // 在【GraphBuilder::finish】阶段被调用，用于对输入option进行再加工
  // 典型场景下GraphVertexBuilder::option设置的origin_option一般是一个描述性配置
  // 可以利用config函数转换成更适宜辅助计算的结构
  // 或者执行一些可以被同一个GraphVertexBuilder对应的多个GraphVertex实例共享的复杂初始化
  // 对应传统意义上的【进程级别初始化】概念
  virtual int config(const ::babylon::Any& origin_option,
                     ::babylon::Any& option) const noexcept;

  // 在【GraphBuilder::build】阶段被调用，首先记录下当前处理的GraphVertex
  // 之后调用按照ANYFLOW_INTERFACE自动生成的__anyflow_declare_interface，声明接口成员
  // 最后调用用户定义的setup根据vertex的option，设置不同的运行模式
  // 对应传统意义上的【线程级别初始化】概念
  int setup(GraphVertex& vertex) noexcept;

  // 在【Graph::run】阶段，激活时调用，一般无需实现
  // 目前仅用于支持内置算子用于推导是否需要声明可变依赖
  virtual int on_activate() noexcept;

  // 在【Graph::run】阶段，执行时调用，首先记录下当前处理的GraphVertex
  // 之后调用按照ANYFLOW_INTERFACE自动生成的__anyflow_prepare_interface，准备接口成员
  // 最后调用用户定义的process完成实际的处理动作
  //
  // 支持同步和异步两种实现模式
  // 同步版本函数返回即处理完成，通过返回值来反馈异常
  // 异步版本函数可提前返回，最终通过closure.done(返回值)来反馈处理完成
  // 返回值!=0表示执行异常，框架会短路终止整个图执行过程
  // 预期内可处理的失败，可以置空输出节点，而非通知异常来表示
  //
  // 一般同步版本可以满足需求，异步版本典型用来对接一些具备异步回调功能的API接口
  // 例如一些IO操作，通过回调串联实现让出当前线程的效果
  void process(GraphVertex& vertex, GraphVertexClosure&& closure) noexcept;
  int process(GraphVertex& vertex) noexcept;

  // 在【Graph::reset】阶段被调用
  // 用于清理临时工作区，准备好下一次运行
  // 和process拆分开可以避免清理重置动作影响主流程
  void reset(GraphVertex& vertex) noexcept;

 protected:
  // 获取节点对应的配置数据
  template <typename T>
  inline const T* option() noexcept;

  // 获取图对应的SwissMemoryResource或直接利用它来创建实例
  // 这个SwissMemoryResource在每次运行完成后会随图生命周期清理
  template <typename T, typename... Args>
  inline T* create_object(Args&&... args) noexcept;
  inline ::babylon::SwissMemoryResource& memory_resource() noexcept;

  // 使用图对应的SwissManager创建可重用实例
  // 这个SwissManager在每次运行完成后会随图生命周期重置
  template <typename T, typename... Args>
  inline ::babylon::ReusableAccessor<T> create_reusable_object(
      Args&&... args) noexcept;

  // 【高级】直接获取算子对应节点
  // 大多常用功能已经通过上面的接口独立封装
  // 取得节点的功能主要用于实现一些不常用功能
  inline GraphVertex& vertex() noexcept;

 private:
  virtual int __anyflow_declare_interface() noexcept;
  virtual int setup() noexcept;

  virtual int __anyflow_prepare_interface() noexcept;
  virtual void process(GraphVertexClosure&& closure) noexcept;
  virtual int process() noexcept;

  virtual void reset() noexcept;

  GraphVertex* _vertex {nullptr};
};

class GraphVertexClosure {
 public:
  // 可以默认构造和移动，不可复制
  inline GraphVertexClosure() noexcept = default;
  inline GraphVertexClosure(GraphVertexClosure&& other) noexcept;
  inline GraphVertexClosure(const GraphVertexClosure&) = delete;
  inline GraphVertexClosure& operator=(GraphVertexClosure&&) noexcept;
  inline GraphVertexClosure& operator=(const GraphVertexClosure&) = delete;
  // 析构时如果没有显式结束，则默认正常结束
  inline ~GraphVertexClosure() noexcept;

  // 为算子包装节点闭包，用生命周期记录运行节点数
  inline GraphVertexClosure(ClosureContext& closure,
                            GraphVertex& vertex) noexcept;

  // 结束运行，并通过error_code == 0标记成功
  inline void done() noexcept;
  inline void done(int error_code) noexcept;

  inline bool finished() const noexcept;

 private:
  ClosureContext* _closure {nullptr};
  GraphVertex* _vertex {nullptr};
};

class Graph;
class GraphData;
class GraphExecutor;
class GraphDependency;
class GraphVertexBuilder;
class GraphVertex {
 private:
  using StringView = ::babylon::StringView;
  using SwissMemoryResource = ::babylon::SwissMemoryResource;

 public:
  // 不会被单独构造，仅提供给Graph中的vector容器使用
  inline GraphVertex() = default;

  // 获取节点基础信息
  inline Graph& graph() noexcept;
  inline StringView name() const noexcept;
  inline size_t index() const noexcept;

  // 根据name获取命名dependency，一般不主动使用
  // 而是用于支持ANYFLOW_INTERFACE宏
  ssize_t index_for_named_dependency(::babylon::StringView name) noexcept;
  inline GraphDependency* named_dependency(size_t index) noexcept;

  // 根据序号获取匿名dependency，范围[0, size)
  inline GraphDependency* anonymous_dependency(size_t index) noexcept;
  inline size_t anonymous_dependency_size() const noexcept;

  // 根据name获取命名emit，一般不主动使用
  // 而是用于支持ANYFLOW_INTERFACE宏
  ssize_t index_for_named_emit(::babylon::StringView name) noexcept;
  inline GraphData* named_emit(size_t index) noexcept;

  // 根据序号获取匿名emit，范围[0, size)
  inline GraphData* anonymous_emit(size_t index) noexcept;
  inline size_t anonymous_emit_size() const noexcept;

  // 获取只读的选项配置，最佳实践是在setup中读取
  // 并将解析后的结果保存到GraphProcessor成员当中
  template <typename T>
  inline const T* option() const noexcept;

  // 标记节点运行为平凡操作
  // 平凡操作在invoke时直接运行而非使用executor
  void declare_trivial() noexcept;

  // 所有emit直接发布空数据，一般不主动使用
  // 而是用于支持ANYFLOW_INTERFACE宏
  void flush_emits() noexcept;

  // 运行callback，供executer在异步环境中调用
  inline void run(GraphVertexClosure&& closure) noexcept;

  template <typename T>
  inline const T* get_graph_context() const noexcept;

 private:
  using DataStack = ::absl::InlinedVector<GraphData*, 128>;
  using VertexStack = ::absl::InlinedVector<GraphVertex*, 128>;

  // 供GraphVertexBuilder使用，用于设置节点信息
  void set_graph(Graph& graph) noexcept;
  void set_builder(const GraphVertexBuilder& builder) noexcept;
  void set_processor(::std::unique_ptr<GraphProcessor>&& processor) noexcept;
  ::std::vector<GraphDependency>& dependencies() noexcept;
  ::std::vector<GraphData*>& emits() noexcept;

  // 供GraphVertexBuilder使用，用于初始化节点
  int setup() noexcept;

  // 供Graph使用，清理执行状态
  void reset() noexcept;

  int activate(DataStack& unsolved_data, VertexStack& runnable_vertexes,
               ClosureContext* closure) noexcept;
  inline bool ready(GraphDependency* denpendency) noexcept;
  inline ClosureContext* closure() noexcept;
  void invoke(VertexStack& runnable_vertexes) noexcept;
  inline VertexStack* runnable_vertexes() noexcept;

 private:
  const GraphVertexBuilder* _builder {nullptr};

  Graph* _graph {nullptr};
  ::std::unique_ptr<GraphProcessor> _processor {nullptr};
  ::std::vector<GraphDependency> _dependencies;
  ::std::vector<GraphData*> _emits;

  bool _trivial {false};
  // bool _finish_with_auto_emit {false};

  ::std::atomic<bool> _activated {false};
  ::std::atomic<int64_t> _waiting_num {0};
  ClosureContext* _closure {nullptr};
  VertexStack* _runnable_vertexes {nullptr};

  friend class Graph;
  friend class GraphData;
  friend class GraphExecutor;
  friend class Closure;
  friend class ClosureContext;
  friend class GraphDependency;
  friend class GraphVertexBuilder;
  friend class GraphVertexClosure;
  friend class GraphBuilder;
};

} // namespace anyflow
BABYLON_NAMESPACE_END

#define ANYFLOW_INTERFACE(members)                              \
  virtual int __anyflow_declare_interface() noexcept override { \
    BOOST_PP_SEQ_FOR_EACH(__ANYFLOW_DECLARE, _, members)        \
    return 0;                                                   \
  }                                                             \
  virtual int __anyflow_prepare_interface() noexcept override { \
    BOOST_PP_SEQ_FOR_EACH(__ANYFLOW_PREPARE, _, members)        \
    return 0;                                                   \
  }                                                             \
  BOOST_PP_SEQ_FOR_EACH(__ANYFLOW_TYPEDEF, _, members)          \
  BOOST_PP_SEQ_FOR_EACH(__ANYFLOW_MEMBER, _, members)

#define __ANYFLOW_TYPEDEF(r, data, args)                       \
  typedef BOOST_PP_REMOVE_PARENS(BOOST_PP_TUPLE_ELEM(1, args)) \
      __ANYFLOW_TYPE_FOR_NAME(BOOST_PP_TUPLE_ELEM(2, args));

#define __ANYFLOW_TYPE_FOR_NAME(name) BOOST_PP_CAT(__hidden_type_for_, name)

#define __ANYFLOW_MEMBER(r, data, args)                        \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(0, args), 0), \
              __ANYFLOW_DEPEND args, __ANYFLOW_EMIT args)

#define __ANYFLOW_DEPEND(r, type, name, is_channel, is_mutable,               \
                         essential_level, is_va_args, ...)                    \
  BOOST_PP_IF(                                                                \
      BOOST_PP_EQUAL(is_va_args, 0), ssize_t __anyflow_index_for_##name {-1}; \
      BOOST_PP_IF(                                                            \
          is_channel,                                                         \
          BOOST_PP_IF(is_mutable, ::babylon::anyflow::MutableChannelConsumer< \
                                      __ANYFLOW_TYPE_FOR_NAME(name)>          \
                                      name;                                   \
                      , ::babylon::anyflow::ChannelConsumer<                  \
                            __ANYFLOW_TYPE_FOR_NAME(name)>                    \
                            name;),                                           \
          BOOST_PP_IF(                                                        \
              is_mutable, __ANYFLOW_TYPE_FOR_NAME(name) * name {nullptr};     \
              , const __ANYFLOW_TYPE_FOR_NAME(name) * name {nullptr};)),      \
      std::vector<const __ANYFLOW_TYPE_FOR_NAME(name)*> name;)

#define __ANYFLOW_EMIT(r, type, name, is_channel, is_mutable, essential_level, \
                       is_va_args, ...)                                        \
  BOOST_PP_IF(                                                                 \
      BOOST_PP_EQUAL(is_va_args, 0), ssize_t __anyflow_index_for_##name {-1};  \
      BOOST_PP_IF(                                                             \
          is_channel,                                                          \
          ::babylon::anyflow::OutputChannel<__ANYFLOW_TYPE_FOR_NAME(name)>     \
              name;                                                            \
          , ::babylon::anyflow::OutputData<__ANYFLOW_TYPE_FOR_NAME(name)>      \
                name;),                                                        \
      std::vector<                                                             \
          ::babylon::anyflow::OutputData<__ANYFLOW_TYPE_FOR_NAME(name)>>       \
          name;)

#define __ANYFLOW_DECLARE(r, data, args)                       \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(0, args), 0), \
              __ANYFLOW_DECLARE_DEPEND args, __ANYFLOW_DECLARE_EMIT args)

#define __ANYFLOW_DECLARE_DEPEND(r, type, name, is_channel, is_mutable,        \
                                 essential_level, is_va_args, ...)             \
  {                                                                            \
    BOOST_PP_IF(                                                               \
        BOOST_PP_EQUAL(is_va_args, 0),                                         \
        auto index = vertex().index_for_named_dependency(#name);               \
        if (index < 0 && essential_level > 0) {                                \
          BABYLON_LOG(WARNING)                                                 \
              << "no depend bind to " #name " for " << vertex();               \
          return -1;                                                           \
        } if (index >= 0) {                                                    \
          auto depend = vertex().named_dependency(static_cast<size_t>(index)); \
          if (is_mutable) {                                                    \
            depend->declare_mutable();                                         \
          }                                                                    \
          depend->declare_essential(essential_level == 1);                     \
          BOOST_PP_IF(                                                         \
              is_channel,                                                      \
              depend->declare_channel<__ANYFLOW_TYPE_FOR_NAME(name)>();        \
              , depend->declare_type<__ANYFLOW_TYPE_FOR_NAME(name)>();)        \
        } __anyflow_index_for_##name = index;                                  \
        ,                                                                      \
        size_t anonymous_depends_size = vertex().anonymous_dependency_size();  \
        name.resize(anonymous_depends_size);                                   \
        for (size_t index = 0; index < anonymous_depends_size; index++) {      \
          auto depend = vertex().anonymous_dependency(index);                  \
          depend->declare_type<__ANYFLOW_TYPE_FOR_NAME(name)>();               \
        })                                                                     \
  }

#define __ANYFLOW_DECLARE_EMIT(r, type, name, is_channel, is_mutable,       \
                               essential_level, is_va_args, ...)            \
  {                                                                         \
    BOOST_PP_IF(                                                            \
        BOOST_PP_EQUAL(is_va_args, 0),                                      \
        auto index = vertex().index_for_named_emit(#name);                  \
        if (index < 0) {                                                    \
          BABYLON_LOG(WARNING)                                              \
              << "no emit bind to " #name " for " << vertex();              \
          return -1;                                                        \
        } auto data = vertex().named_emit(index);                           \
        BOOST_PP_IF(is_channel,                                             \
                    data->declare_channel<__ANYFLOW_TYPE_FOR_NAME(name)>(); \
                    , data->declare_type<__ANYFLOW_TYPE_FOR_NAME(name)>();) \
            __anyflow_index_for_##name = index;                             \
        , size_t anonymous_emits_size = vertex().anonymous_emit_size();     \
        name.resize(anonymous_emits_size);                                  \
        for (size_t index = 0; index < anonymous_emits_size; index++) {     \
          auto data = vertex().anonymous_emit(index);                       \
          data->declare_type<__ANYFLOW_TYPE_FOR_NAME(name)>();              \
        })                                                                  \
  }

#define __ANYFLOW_PREPARE(r, data, args)                       \
  BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(0, args), 0), \
              __ANYFLOW_PREPARE_DEPEND args, __ANYFLOW_PREPARE_EMIT args)

#define __ANYFLOW_PREPARE_DEPEND(r, type, name, is_channel, is_mutable,        \
                                 essential_level, is_va_args, ...)             \
  {                                                                            \
    BOOST_PP_IF(                                                               \
        BOOST_PP_EQUAL(is_va_args, 0),                                         \
        if (__anyflow_index_for_##name >= 0) {                                 \
          auto depend = vertex().named_dependency(__anyflow_index_for_##name); \
          BOOST_PP_IF(                                                         \
              is_channel,                                                      \
              BOOST_PP_IF(                                                     \
                  is_mutable,                                                  \
                  auto channel =                                               \
                      depend                                                   \
                          ->mutable_channel<__ANYFLOW_TYPE_FOR_NAME(name)>();  \
                  , auto channel =                                             \
                        depend->channel<__ANYFLOW_TYPE_FOR_NAME(name)>();)     \
                  name = channel.subscribe();                                  \
              ,                                                                \
              BOOST_PP_IF(                                                     \
                  is_mutable,                                                  \
                  name =                                                       \
                      depend->mutable_value<__ANYFLOW_TYPE_FOR_NAME(name)>();  \
                  , name = depend->value<__ANYFLOW_TYPE_FOR_NAME(name)>();))   \
          BOOST_PP_IF(                                                         \
              BOOST_PP_GREATER(essential_level, 0),                            \
              if (!static_cast<bool>(name)) {                                  \
                BABYLON_LOG(WARNING) << "depend data " #name " is empty";      \
                return -1;                                                     \
              }, )                                                             \
        } else {                                                               \
          BOOST_PP_IF(                                                         \
              is_channel,                                                      \
              BOOST_PP_IF(is_mutable, ::babylon::anyflow::MutableInputChannel< \
                                          __ANYFLOW_TYPE_FOR_NAME(name)>       \
                                          channel;                             \
                          , ::babylon::anyflow::InputChannel<                  \
                                __ANYFLOW_TYPE_FOR_NAME(name)>                 \
                                channel;) name = channel.subscribe();          \
              , name = nullptr;)                                               \
        },                                                                     \
        size_t anonymous_depends_size = vertex().anonymous_dependency_size();  \
        for (size_t index = 0; index < anonymous_depends_size; index++) {      \
          auto depend = vertex().anonymous_dependency(index);                  \
          name.at(index) = depend->value<__ANYFLOW_TYPE_FOR_NAME(name)>();     \
          BOOST_PP_IF(                                                         \
              BOOST_PP_GREATER(essential_level, 0),                            \
              if (!static_cast<bool>(name.at(index))) {                        \
                BABYLON_LOG(WARNING)                                           \
                    << "anonymous depend data " #name " is empty";             \
                return -1;                                                     \
              }, )                                                             \
        })                                                                     \
  }

#define __ANYFLOW_PREPARE_EMIT(r, type, name, is_channel, is_mutable,     \
                               essential_level, is_va_args, ...)          \
  {                                                                       \
    BOOST_PP_IF(                                                          \
        BOOST_PP_EQUAL(is_va_args, 0),                                    \
        auto emit = vertex().named_emit(__anyflow_index_for_##name);      \
        BOOST_PP_IF(                                                      \
            is_channel,                                                   \
            name = emit->output_channel<__ANYFLOW_TYPE_FOR_NAME(name)>(); \
            , name = emit->output<__ANYFLOW_TYPE_FOR_NAME(name)>();),     \
        size_t anonymous_emits_size = vertex().anonymous_emit_size();     \
        for (size_t index = 0; index < anonymous_emits_size; index++) {   \
          auto emit = vertex().anonymous_emit(index);                     \
          name.at(index) = emit->output<__ANYFLOW_TYPE_FOR_NAME(name)>(); \
        })                                                                \
  }

#define ANYFLOW_DEPEND_DATA(type, name, ...) \
  ANYFLOW_DEPEND((type, name, 0, 0, ##__VA_ARGS__))
#define ANYFLOW_DEPEND_MUTABLE_DATA(type, name, ...) \
  ANYFLOW_DEPEND((type, name, 0, 1, ##__VA_ARGS__))
#define ANYFLOW_DEPEND_CHANNEL(type, name, ...) \
  ANYFLOW_DEPEND((type, name, 1, 0, ##__VA_ARGS__))
#define ANYFLOW_DEPEND_MUTABLE_CHANNEL(type, name, ...) \
  ANYFLOW_DEPEND((type, name, 1, 1, ##__VA_ARGS__))
#define ANYFLOW_DEPEND_VA_DATA(type, va_args_vec, ...) \
  ANYFLOW_DEPEND((type, va_args_vec, 0, 0, 2, 1))
#define ANYFLOW_DEPEND(args) \
  (__ANYFLOW_FILL_ARGS(BOOST_PP_TUPLE_PUSH_FRONT(args, 0)))

#define ANYFLOW_EMIT_DATA(type, name, ...) ANYFLOW_EMIT((type, name, 0))
#define ANYFLOW_EMIT_CHANNEL(type, name, ...) ANYFLOW_EMIT((type, name, 1))
#define ANYFLOW_EMIT_VA_DATA(type, va_args_vec, ...) \
  ANYFLOW_EMIT((type, va_args_vec, 0, 0, 2, 1))
#define ANYFLOW_EMIT(args) \
  (__ANYFLOW_FILL_ARGS(BOOST_PP_TUPLE_PUSH_FRONT(args, 1)))

#define __ANYFLOW_FILL_ARGS(args)                           \
  __ANYFLOW_FILL_IS_VA_ARGS(__ANYFLOW_FILL_ESSENTIAL_LEVEL( \
      __ANYFLOW_FILL_IS_MUTABLE(__ANYFLOW_FILL_IS_CHANNEL(args))))

#define __ANYFLOW_FILL_IS_VA_ARGS(args)                    \
  BOOST_PP_IF(BOOST_PP_LESS(BOOST_PP_TUPLE_SIZE(args), 7), \
              BOOST_PP_TUPLE_PUSH_BACK(args, 0), args)

#define __ANYFLOW_FILL_ESSENTIAL_LEVEL(args)               \
  BOOST_PP_IF(BOOST_PP_LESS(BOOST_PP_TUPLE_SIZE(args), 6), \
              BOOST_PP_TUPLE_PUSH_BACK(args, 2), args)

#define __ANYFLOW_FILL_IS_MUTABLE(args)                    \
  BOOST_PP_IF(BOOST_PP_LESS(BOOST_PP_TUPLE_SIZE(args), 5), \
              BOOST_PP_TUPLE_PUSH_BACK(args, 0), args)

#define __ANYFLOW_FILL_IS_CHANNEL(args)                    \
  BOOST_PP_IF(BOOST_PP_LESS(BOOST_PP_TUPLE_SIZE(args), 4), \
              BOOST_PP_TUPLE_PUSH_BACK(args, 0), args)

#include "babylon/anyflow/vertex.hpp"
