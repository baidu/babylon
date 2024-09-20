#pragma once

#include "babylon/any.h"
#include "babylon/concurrent/transient_topic.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/container/inlined_vector.h)
// clang-format on

#include <vector>

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

// 两阶段发布模式的提交器
// 用于操作待发布数据
// 并控制发布的最终提交生效
class GraphData;
template <typename T>
class Committer {
 public:
  // 从data创建commiter，创建时会竞争data，只有唯一valid
  inline Committer(GraphData& data) noexcept;
  // 析构时候自动提交，不可复制
  inline Committer(const Committer& other) = delete;
  // 可以移动构造，拆函数，异步等场景，控制延迟提交
  inline Committer(Committer&& other) noexcept;
  inline Committer& operator=(Committer&& other) noexcept;
  // 析构时自动提交
  inline ~Committer() noexcept;
  // 可用性判断，访问对象前先判断一下
  // 只有valid的commiter可以访问数据
  inline operator bool() const noexcept;
  inline bool valid() const noexcept;
  // 可用时可以操作数据
  inline T* get() noexcept;
  inline T* operator->() noexcept;
  inline T& operator*() noexcept;
  // 引用输出value
  // 尽量保持持续使用引用，或者持续使用本体
  // 两者之间切换会造成本体被清空，无法复用
  inline void ref(T& value) noexcept;
  inline void ref(const T& value) noexcept;
  inline void cref(const T& value) noexcept;
  // 默认发布数据非空，如果需要发布空数据
  // 需要主动调用clear，不会实际清除底层数据
  inline void clear() noexcept;
  // 主动提交data，之后无法再进行操作
  inline void release() noexcept;
  // 取消发布
  inline void cancel() noexcept;

 private:
  GraphData* _data {nullptr};
  bool _valid {false};
  bool _keep_reference {false};
};

// 通过ANYFLOW_EMIT_DATA(type, name, ...)产生
// OutputData<type> name;
// 类型的成员变量，提供给GraphProcessor用来发布数据
// 实际发布过程采用两阶段提交模式
// Committer<type> committer = name.emit();
// ... // 通过committer操作实际数据结构
// committer.release(); // 结束发布并提交
template <typename T>
class OutputData {
 public:
  inline OutputData() noexcept = default;
  inline OutputData(OutputData&&) noexcept = default;
  inline OutputData(const OutputData&) noexcept = default;
  inline OutputData& operator=(OutputData&&) noexcept = default;
  inline OutputData& operator=(const OutputData&) noexcept = default;
  inline ~OutputData() noexcept = default;

  // 是否绑定了有效的GraphData用于数据发布
  // 一般通过ANYFLOW_EMIT_DATA宏产生的成员必定有效，无需检测
  inline operator bool() const noexcept;

  template <typename U>
  inline OutputData& operator=(U&& value) noexcept;

  // 【GraphProcessor::setup】阶段使用，注册清理函数
  // 当Graph::reset时，会调用callback(T&)来完成清理
  // 用户未设置时，会根据T的情况进行默认处理
  // 1、如果存在T::reset()或者T::clear()，则用来做清理
  // 2、否则将持有T销毁
  template <typename C>
  inline void set_on_reset(C&& callback) noexcept;

  // 【GraphProcessor::process】阶段使用，发布数据
  // 获取到的数据发布器类似xx_ptr<T>可以用于操作待发布数据
  // 析构或明确的Committer<T>::release调用时发布当前的操作结果
  inline Committer<T> emit() noexcept;

 private:
  inline OutputData(GraphData& data) noexcept;

  GraphData* _data {nullptr};

  friend class GraphData;
};

template <typename T>
class OutputChannel;
template <typename T>
class ChannelPublisher {
 private:
  using Topic = ::babylon::ConcurrentTransientTopic<T>;

 public:
  using Iterator = typename Topic::Iterator;

  // 通过生命周期管理发布流程
  // 析构时自动关闭发布流
  inline ChannelPublisher() noexcept = default;
  inline ChannelPublisher(ChannelPublisher&& other) noexcept;
  inline ChannelPublisher(const ChannelPublisher&) noexcept = delete;
  inline ChannelPublisher& operator=(ChannelPublisher&& other) noexcept;
  inline ChannelPublisher& operator=(const ChannelPublisher&) noexcept = delete;
  inline ~ChannelPublisher() noexcept;

  template <typename U>
  inline void publish(U&& value) noexcept;

  template <typename C>
  inline void publish_n(size_t num, C&& callback) noexcept;

  // 关闭发布流，通知消费者发布结束
  inline void close() noexcept;

 private:
  inline ChannelPublisher(Topic& topic, GraphData& data) noexcept;

  Topic* _topic {nullptr};
  GraphData* _data {nullptr};

  friend class OutputChannel<T>;
};

template <typename T>
class OutputChannel {
 private:
  using Topic = ::babylon::ConcurrentTransientTopic<T>;

 public:
  inline OutputChannel() noexcept = default;
  inline OutputChannel(OutputChannel&&) noexcept = default;
  inline OutputChannel(const OutputChannel&) noexcept = default;
  inline OutputChannel& operator=(OutputChannel&&) noexcept = default;
  inline OutputChannel& operator=(const OutputChannel&) noexcept = default;
  inline ~OutputChannel() noexcept = default;

  // 是否绑定了有效的GraphData用于数据发布
  // 一般通过ANYFLOW_EMIT_CHANNEL宏产生的成员必定有效，无需检测
  inline operator bool() const noexcept;

  // 开启发布流，可以通过返回的发布器进一步完成流式发布
  // 返回的发布器在析构时自动关闭发布流
  inline ChannelPublisher<T> open() noexcept;

 private:
  inline OutputChannel(GraphData& data) noexcept;

  GraphData* _data {nullptr};

  friend class GraphData;
};

// 连接vertex的中间环节，也是之间交互数据的存放位置
// 对processor承诺，当检测ready之后，可以安全使用数据而不会有竞争风险
// 限定data的写入发布只有一次，因此整个过程无需上锁
// 保证release-acquire衔接即可
//
// 独立的emply标记支持保留data对象的前提下，表达空语义
//
// 声明为可变依赖时稍微特殊，ready之后还会由依赖者修改数据
// 通过保证此时只有唯一依赖者，来满足承诺
class GraphVertex;
class GraphExecutor;
class GraphDependency;
class ClosureContext;
template <typename T>
class OutputChannel;
template <typename T>
class ChannelPublisher;
template <typename T>
class InputChannel;
template <typename T>
class MutableInputChannel;
class Graph;
class GraphData {
 public:
  using Any = ::babylon::Any;
  using DataStack = ::absl::InlinedVector<GraphData*, 128>;
  using VertexStack = ::absl::InlinedVector<GraphVertex*, 128>;
  using OnResetFunction = ::std::function<void(Any&)>;

  // 禁用拷贝和移动构造
  inline GraphData() = default;
  inline GraphData(GraphData&&) = delete;
  inline GraphData(const GraphData&) = delete;
  inline GraphData& operator=(GraphData&&) = delete;
  inline GraphData& operator=(const GraphData&) = delete;

  // 【GraphProcessor::setup】阶段使用，用于声明并校验变量类型一致性
  // 如果出现多次声明间类型不一致，返回无效OutputData
  // 且最终GraphBuilder::build函数也会返回失败
  //
  // 一般无需主动调用，而是通过ANYFLOW_INTERFACE生成代码自动调用
  // T == Any特化：忽视本次调用
  template <typename T>
  inline OutputData<T> declare_type() noexcept;

  // 【GraphProcessor::setup】阶段使用，用于声明并校验流一致性
  // 如果出现多次声明间类型不一致，返回无效OutputChannel
  // 且最终GraphBuilder::build函数也会返回失败
  //
  // 一般无需主动调用，而是通过ANYFLOW_INTERFACE生成代码自动调用
  template <typename T>
  inline OutputChannel<T> declare_channel() noexcept;

  // 【GraphProcessor::on_activate】【GraphProcessor::process】阶段使用
  // 检查是否需要可变发布，即被下游可变依赖
  inline bool need_mutable() const noexcept;

  // 【Graph::run】之前
  // 【GraphProcessor::process】阶段使用
  // 尝试写操作并发布，获得操作权会返回valid的Committer
  // T == Any 特化：返回包装底层容器的Committer，用于高级操作
  template <typename T>
  inline Committer<T> emit() noexcept;

  // 【GraphProcessor::process】阶段使用
  // 转发一个依赖，优先直接引用输出
  // 仅在需要输出可变，即有下游声明了可变依赖
  // 且dependency不支持时采用拷贝
  inline bool forward(GraphDependency& dependency) noexcept;

  // 【Graph::run】之前
  // 设置data的预置值，主要用于从外部提供构造好的实例，供真正emit时复用
  // 典型如RPC场景下，response已经由RPC框架构造好，可以通过preset注入进来避免拷贝
  template <typename T>
  inline void preset(T& value) noexcept;
  inline bool has_preset_value() const noexcept;

  // 【Graph::run】前后
  // 获得只读的value，发布者应该使用emit进行写操作
  template <typename T>
  inline const T* value() const noexcept;
  template <typename T>
  inline const T* cvalue() const noexcept;
  // 对基本类型进行隐式转换，相当于static_cast
  template <typename T>
  inline T as() const noexcept;

  // 【Graph::run】前后
  // data是否已经发布
  inline bool ready() const noexcept;
  // data是否为空
  inline bool empty() const noexcept;

  inline const ::std::string& name() const noexcept;

  inline const ::babylon::Id& declared_type() const noexcept;

  template <typename T>
  inline OutputData<T> output() noexcept;
  template <typename T>
  inline OutputChannel<T> output_channel() noexcept;

 private:
  BABYLON_DECLARE_MEMBER_INVOCABLE(clear, Clearable)
  BABYLON_DECLARE_MEMBER_INVOCABLE(reset, Resetable)

  template <typename T>
  inline void set_default_on_reset() noexcept;
  template <typename T,
            typename ::std::enable_if<Resetable<T>::value, int>::type = 0>
  static void default_on_reset(Any& data) noexcept;
  template <typename T,
            typename ::std::enable_if<
                !Resetable<T>::value && Clearable<T>::value, int>::type = 0>
  static void default_on_reset(Any& data) noexcept;
  template <typename T,
            typename ::std::enable_if<
                !Resetable<T>::value && !Clearable<T>::value, int>::type = 0>
  static void default_on_reset(Any& data) noexcept;

  template <typename T, typename C>
  inline void set_on_reset(C&& callback) noexcept;

  template <typename T>
  inline static T* switch_to(Any& data) noexcept;

  void set_name(const ::std::string& name) noexcept;
  void set_graph(const Graph& graph) noexcept;
  inline void executer(GraphExecutor& executer) noexcept;
  inline void producer(GraphVertex& producer) noexcept;
  inline void add_successor(GraphDependency& successor) noexcept;
  inline void data_num(size_t num) noexcept;
  inline void vertex_num(size_t num) noexcept;
  inline GraphVertex* producer() noexcept;
  inline const GraphVertex* producer() const noexcept;
  inline std::vector<GraphVertex*>* producers() noexcept;
  inline const std::vector<GraphVertex*>* producers() const noexcept;

  // check声明变量类型，如果发生声明不一致，返回false
  // 而且，最终graph builder的build方法会返回失败
  // T == Any 特化：无效果，恒返回true
  template <typename T>
  inline OutputData<T> check_declare_type() noexcept;

  // 检查是否是最后一个producer，针对channel类data在所有producer发布完成后进行close
  // queue
  inline bool check_last_producer() noexcept;

  inline int32_t error_code() const noexcept;
  // 重置状态，但是保留data空间
  inline void reset() noexcept;
  // 获取发布权，竞争下只有第一次返回true，此时进入非空状态
  // 只有获得发布权后才可以进一步写操作value，以及release
  // 如果要发布空value，需要主动标记empty(true)
  // 整套操作封装成Committer呈现
  inline bool acquire() noexcept;
  // 发布data，递减等待data的closure的计数
  // 之后依次通知successor
  void release() noexcept;
  // 获取可写value指针，通过GraphDependency同名函数间接提供
  // 写入者要通过使用commiter来遵守流程
  template <typename T>
  inline T* mutable_value() noexcept;
  // 获取除了可写value指针，确保底层存储为T类型的非引用对象
  // 如果不是，则重新使用T()创建对象并置入底层容器
  // 写入者要通过使用commiter来遵守流程
  template <typename T,
            typename ::std::enable_if<!::std::is_move_constructible<T>::value,
                                      int32_t>::type = 0>
  inline T* certain_type_non_reference_mutable_value() noexcept;
  template <typename T,
            typename ::std::enable_if<::std::is_move_constructible<T>::value,
                                      int32_t>::type = 0>
  inline T* certain_type_non_reference_mutable_value() noexcept;
  template <typename T>
  inline void ref(T& value) noexcept;
  template <typename T>
  inline void cref(const T& value) noexcept;
  // 标记data为空，不会清除实际数据
  inline void empty(bool empty) noexcept;
  // 绑定一个closure
  // 尚未ready时，增加closure的等待数
  // 并记录closure，之后返回true，当data ready时进行通知
  // 如果data已经ready，则直接返回false
  inline bool bind(ClosureContext& closure) noexcept;

  // 由GraphDependency使用
  // 调用后进入不可变依赖状态
  // true：调用前无依赖或者不可变依赖
  // false：调用前可变依赖
  inline bool acquire_immutable_depend() noexcept;
  // 由GraphDependency使用
  // 调用后进入可变依赖状态
  // true：调用前无依赖
  // false：调用前可变依赖或者不可变依赖
  inline bool acquire_mutable_depend() noexcept;
  // 激活标记，只用于推导阶段去重，没有使用原子操作
  // 因此，多线程下不保证只会active一次，只用于初筛
  inline bool mark_active() noexcept;
  // 以当前data为根，按照依赖路径递归激活
  // 最终新增需要发起执行的vertex会收集在runnable_vertexes中
  int32_t recursive_activate(VertexStack& runnable_vertexes,
                             ClosureContext* closure) noexcept;
  // 尝试触发到激活状态，需要激活的会将自身加入activating_data
  inline void trigger(DataStack& activating_data) noexcept;
  // 激活当前data，如果producer依赖就绪，则加入runnable_vertexes
  // 如果producer还有依赖，则进入激活状态，并将进一步需要激活的节点
  // 加入activating_data
  int activate(DataStack& activating_data, VertexStack& runnable_vertexes,
               ClosureContext* closure) noexcept;

  bool check_safe_mutable() const noexcept;

  static ClosureContext* SEALED_CLOSURE;

  // 静态信息
  ::std::string _name;
  const Graph* _graph {nullptr};
  ::std::vector<GraphVertex*> _producers;
  GraphVertex* _producer {nullptr};
  ::std::vector<GraphDependency*> _successors;
  GraphExecutor* _executer {nullptr};
  size_t _data_num {0};
  size_t _vertex_num {0};
  const ::babylon::Id* _declared_type {&::babylon::TypeId<Any>::ID};
  bool _error_code {0};

  // 数据信息
  ::std::atomic<bool> _acquired {false};
  Any _data;
  bool _empty {true};
  bool _has_preset_value {false};

  // 推导信息
  bool _active {false};
  ::std::atomic<ClosureContext*> _closure {nullptr};
  ::std::atomic<int32_t> _depend_state {0};
  ::std::atomic<uint32_t> _producer_done_num {0};

  OnResetFunction _on_reset {default_on_reset<::babylon::NeverUsed>};

  template <typename T>
  friend class Committer;
  template <typename T>
  friend class OutputData;
  template <typename T>
  friend class OutputChannel;
  template <typename T>
  friend class ChannelPublisher;
  template <typename T>
  friend class InputChannel;
  template <typename T>
  friend class MutableInputChannel;
  friend ::std::ostream& operator<<(::std::ostream&, const GraphData&);
  friend class Graph;
  friend class GraphBuilder;
  friend class ClosureContext;
  friend class GraphDependency;
  friend class GraphDependencyBuilder;
  friend class GraphEmitBuilder;
  friend class GraphVertex;
  friend class GraphVertexBuilder;
};

} // namespace anyflow
BABYLON_NAMESPACE_END

#include "babylon/anyflow/data.hpp"
