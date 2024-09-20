#pragma once

#include "babylon/any.h"
#include "babylon/concurrent/transient_topic.h"

#include "absl/container/inlined_vector.h"

#include <atomic>

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

template <typename T>
class InputChannel;
template <typename T>
class MutableInputChannel;
class GraphData;
class GraphVertex;
class GraphDependency {
 public:
  GraphDependency() noexcept = default;
  GraphDependency(const GraphDependency&) noexcept = delete;
  GraphDependency(GraphDependency&&) noexcept;

  // 【GraphProcessor::setup】【GraphProcessor::on_activate】阶段使用
  // 声明依赖可变性，只有声明为可变的依赖才可以通过mutable_value|mutable_channel修改依赖数据
  // 一般用于支持在连续串行阶段流水线式共享修改一份实体数据，避免拷贝
  //
  // 为了保证一致性，存在可变依赖的情况下，依赖目标的GraphData不能再存在其他依赖，即使是不可变的
  // 打破唯一性约束的情况下，Graph::run会返回失败
  //
  // 在下一次声明前，会维持之前的设定，reset也不会清除
  //
  // 一般无需主动调用，而是通过ANYFLOW_INTERFACE生成代码自动调用
  inline void declare_mutable() noexcept;
  inline bool is_mutable() const noexcept;

  // 【GraphProcessor::setup】阶段使用，用于声明变量类型
  // 如果出现多次声明间类型不一致，最终GraphBuilder::build会失败
  //
  // 一般无需主动调用，而是通过ANYFLOW_INTERFACE生成代码自动调用
  template <typename T>
  inline void declare_type() noexcept;

  // 【GraphProcessor::setup】阶段使用，用于声明流类型
  // 如果出现多次声明间类型不一致，最终GraphBuilder::build会失败
  //
  // 一般无需主动调用，而是通过ANYFLOW_INTERFACE生成代码自动调用
  template <typename T>
  inline void declare_channel() noexcept;

  // 【GraphProcessor::setup】【GraphProcessor::on_activate】阶段使用
  // 声明强制依赖，在下一次声明前，会维持之前的设定，reset也不会清除
  // 如果强制依赖，则condition不成立或者target发布为空时，均不再触发下游算子执行，下游算子直接发布空数据
  inline void declare_essential(bool is_essential = false) noexcept;
  inline bool is_essential() const noexcept;

  // 【GraphProcessor::process】阶段使用
  // 依赖已经就绪，可以发起取值了
  // 一般无需单独检测，目前只有条件依赖可能在process时尚未ready
  inline bool ready() const noexcept;
  // 依赖成立，即condition ready且取值为true，或者无condition
  // 目前成立和就绪当process执行时已经是等同的了
  inline bool established() const noexcept;
  // 目标值为空，此时value和mutable_value返回nullptr
  // 可以根据此状态检测是否为空还是类型不匹配
  // 不关心这个区别细分的一般无需特意调用
  inline bool empty() const noexcept;
  // 在ready&&!empty时返回目标值指针
  // T == Any 特化：返回目标底层Any容器的指针，用于多类型支持等高级场景
  template <typename T>
  inline const T* value() const noexcept;
  template <typename T>
  inline T as() const noexcept;
  // 在ready&&!empty&&is_mutable时返回常量目标值指针
  // T == Any 特化：返回目标底层Any容器的指针，用于多类型支持等高级场景
  template <typename T>
  inline T* mutable_value() noexcept;
  int activated_vertex_name(
      ::std::vector<std::string>& vertex_name) const noexcept;
  int activated_vertex_name(std::string& vertex_name) const noexcept;
  ///////////////////////////////////////////////////////////////////////////

  template <typename T>
  inline InputChannel<T> channel() noexcept;
  template <typename T>
  inline MutableInputChannel<T> mutable_channel() noexcept;

 private:
  using DataStack = ::absl::InlinedVector<GraphData*, 128>;
  using VertexStack = ::absl::InlinedVector<GraphVertex*, 128>;

  inline void source(GraphVertex& vertex) noexcept;
  inline void target(GraphData& data) noexcept;
  inline void condition(GraphData& data, bool establish_value) noexcept;

  inline void reset() noexcept;
  // return 0: 正常激活，进入等待状态
  //        1: 正常激活，进入完成状态
  //       <0: 激活失败
  int activate(DataStack& activating_data) noexcept;
  void ready(GraphData* data, VertexStack& runnable_vertexes) noexcept;
  // 检测依赖是否成立，实际读取原子变量
  // 如果依赖成立，设置_established供后续使用
  inline bool check_established() noexcept;
  inline GraphData* target() noexcept;
  inline const GraphData* inner_condition() const noexcept;
  inline const GraphData* inner_target() const noexcept;

  // 【GraphProcessor::setup】阶段使用
  // 声明预期数据类型，如果多个依赖方对同一个GraphData的类型预期不一致，靠后的调用返回false
  // 发生不一致之后，无论返回值是否被处理，最终GraphBuilder::build会失败
  template <typename T>
  inline void check_declare_type() noexcept;

  GraphVertex* _source {nullptr};
  GraphData* _target {nullptr};
  GraphData* _condition {nullptr};
  bool _establish_value {false};
  bool _mutable {false};
  // bool _critical = true;
  // int64_t _ttl = -1;
  // 等待可用的data数目
  // 激活时会根据有无condition进行+1或+2
  // target可用后，会无条件-1，不管此时是否激活
  // condition可用后，会先无条件-1，不管此时是否激活
  // 如果condition不成立，终值不为0会再进行一次-1，不管此时是否激活
  // 重置后初始值为0，取值范围是[-3, -2, -1, 0, 1, 2]
  // 1、可用的后续操作皆为减法，而激活操作的加法确保只有一次
  // 因此终值为0的操作确保至多只有一次，且能够表征
  // 【已经激活】且【依赖已经就绪】
  // 2、由于减法皆为-1，不经过终值0，也能表征激活且就绪的场景
  // 一定以激活+2收尾（-1 -1 -1 +2）
  // 因此追加终值为-1的激活操作也记入终止状态
  // 1 + 2规则描述了唯一的终止态的操作，此时可以向vertex发起依赖就绪
  // 3、对于condition可用操作，终值为1表示已经激活但target尚未可用
  // 此时如果condition成立，则尝试激活target（激活data是幂等操作）
  // 4、对于激活操作，终值为1，需要检测condition是否可用
  // 如果可用，则尝试激活target（激活data是幂等操作）
  ::std::atomic<int64_t> _waiting_num {0};
  // 依赖是否有效
  // 当条件就绪且成立后修改成true
  // 只有有效的依赖，才能进一步获取到值
  bool _established {false};
  bool _ready {false};

  // 是否强依赖，如果强依赖则要求target不为空才能触发_source
  bool _essential {false};

  friend class GraphData;
  friend class GraphVertex;
  friend class ClosureContext;
  friend class GraphDependencyBuilder;
  template <typename T>
  friend class InputChannel;
  template <typename T>
  friend class MutableInputChannel;
};

template <typename T>
class ChannelConsumer {
 private:
  using Topic = ::babylon::ConcurrentTransientTopic<T>;

 public:
  using Consumer = typename Topic::ConstConsumer;
  using ConsumeRange = typename Topic::ConstConsumeRange;

  // Topic::ConstConsumer的轻量级包装，可以默认构造和移动
  inline ChannelConsumer() noexcept = default;
  inline ChannelConsumer(ChannelConsumer&&) noexcept = default;
  inline ChannelConsumer(const ChannelConsumer&) noexcept = delete;
  inline ChannelConsumer& operator=(ChannelConsumer&& other) noexcept = default;
  inline ChannelConsumer& operator=(const ChannelConsumer&) noexcept = delete;
  inline ~ChannelConsumer() noexcept = default;

  inline operator bool() const noexcept {
    return _valid;
  }

  inline const T* consume() {
    return _consumer.consume();
  }

  inline ConsumeRange consume(size_t num) {
    return _consumer.consume(num);
  }

 private:
  inline ChannelConsumer(Consumer&& consumer, bool valid) noexcept
      : _consumer(::std::move(consumer)), _valid(valid) {}

  Consumer _consumer;
  bool _valid;

  friend class InputChannel<T>;
};

template <typename T>
class InputChannel {
 private:
  using Topic = ::babylon::ConcurrentTransientTopic<T>;

 public:
  // GraphDependency的轻量级包装，可以默认构造和拷贝移动
  inline InputChannel() noexcept = default;
  inline InputChannel(const InputChannel&) noexcept = default;
  inline InputChannel& operator=(const InputChannel&) noexcept = default;

  // 开启订阅流，可以通过返回的消费器进一步完成流式订阅
  inline ChannelConsumer<T> subscribe() {
    auto& queue = value();
    return ChannelConsumer<T>(queue.subscribe(),
                              &queue != DEFAULT_CLOSED_EMPTY_QUEUE.get());
  }

  inline const Topic& value() {
    if (_dependency == nullptr) {
      return *(DEFAULT_CLOSED_EMPTY_QUEUE.get());
    }
    auto* queue = _dependency->value<Topic>();
    if (queue == nullptr) {
      queue = DEFAULT_CLOSED_EMPTY_QUEUE.get();
    }
    return *queue;
  }

 private:
  inline InputChannel(GraphDependency& dependency) noexcept
      : _dependency(&dependency) {}

  static const ::std::unique_ptr<Topic> DEFAULT_CLOSED_EMPTY_QUEUE;

  GraphDependency* _dependency {nullptr};

  friend class GraphDependency;
};

template <typename T>
class MutableChannelConsumer {
 private:
  using Topic = ::babylon::ConcurrentTransientTopic<T>;

 public:
  using Consumer = typename Topic::Consumer;
  using ConsumeRange = typename Topic::ConsumeRange;

  // Topic::Consumer的轻量级包装，可以默认构造和移动
  inline MutableChannelConsumer() noexcept = default;
  inline MutableChannelConsumer(MutableChannelConsumer&&) noexcept = default;
  inline MutableChannelConsumer(const MutableChannelConsumer&) noexcept =
      delete;
  inline MutableChannelConsumer& operator=(MutableChannelConsumer&&) noexcept =
      default;
  inline MutableChannelConsumer& operator=(
      const MutableChannelConsumer&) noexcept = delete;
  inline ~MutableChannelConsumer() noexcept = default;

  inline operator bool() const noexcept {
    return _valid;
  }

  inline T* consume() {
    return _consumer.consume();
  }

  inline ConsumeRange consume(size_t num) {
    return _consumer.consume(num);
  }

 private:
  inline MutableChannelConsumer(Consumer&& consumer, bool valid) noexcept
      : _consumer(::std::move(consumer)), _valid(valid) {}

  Consumer _consumer;
  bool _valid;

  friend class MutableInputChannel<T>;
};

template <typename T>
class MutableInputChannel {
 private:
  using Topic = ::babylon::ConcurrentTransientTopic<T>;

 public:
  // GraphDependency的轻量级包装，可以默认构造和拷贝移动
  inline MutableInputChannel() noexcept = default;
  inline MutableInputChannel(const MutableInputChannel&) noexcept = default;
  inline MutableInputChannel& operator=(const MutableInputChannel&) noexcept =
      default;

  inline MutableChannelConsumer<T> subscribe() {
    auto& queue = value();
    return MutableChannelConsumer<T>(
        queue.subscribe(), &queue != DEFAULT_CLOSED_EMPTY_QUEUE.get());
  }

  inline Topic& value() {
    auto* queue = _dependency->mutable_value<Topic>();
    if (queue == nullptr) {
      queue = DEFAULT_CLOSED_EMPTY_QUEUE.get();
    }
    return *queue;
  }

 private:
  inline MutableInputChannel(GraphDependency& dependency) noexcept
      : _dependency(&dependency) {}

  static const ::std::unique_ptr<Topic> DEFAULT_CLOSED_EMPTY_QUEUE;

  GraphDependency* _dependency {nullptr};

  friend class GraphDependency;
};

template <typename T>
const ::std::unique_ptr<typename InputChannel<T>::Topic>
    InputChannel<T>::DEFAULT_CLOSED_EMPTY_QUEUE = [] {
      auto* topic = new typename InputChannel<T>::Topic;
      topic->close();
      return ::std::unique_ptr<typename InputChannel<T>::Topic>(topic);
    }();

template <typename T>
const ::std::unique_ptr<typename MutableInputChannel<T>::Topic>
    MutableInputChannel<T>::DEFAULT_CLOSED_EMPTY_QUEUE = [] {
      auto* topic = new typename MutableInputChannel<T>::Topic;
      topic->close();
      return ::std::unique_ptr<typename MutableInputChannel<T>::Topic>(topic);
    }();

} // namespace anyflow
BABYLON_NAMESPACE_END

#include "babylon/anyflow/dependency.hpp"
