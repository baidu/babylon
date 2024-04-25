#pragma once

#include "babylon/anyflow/closure.h"

#include "babylon/executor.h"
#include "babylon/move_only_function.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

// 图执行器，采用相应调度机制来执行一个图节点
class Closure;
class GraphVertex;
class ClosureContext;
class GraphVertexClosure;
class GraphExecutor {
public:
    GraphExecutor() = default;
    virtual ~GraphExecutor() noexcept {}
    // 创建一个适应相应调度机制的closure
    virtual Closure create_closure() noexcept = 0;
    // 使用相应的调度机制执行一个vertex
    // 通过GraphVertexClosure的生命周期来表达执行结束
    // 返回非0标识未能完成调度，此时确保vertex未被执行
    // 且closure依旧可用
    // 注：不在此处进行fail标记，避免死锁
    virtual int32_t run(GraphVertex* vertex,
        GraphVertexClosure&& closure) noexcept = 0;
    // 使用相应的调度机制执行一个closure的callback
    // 返回非0标识未能完成调度，此时确保callback未被执行
    virtual int32_t run(ClosureContext* closure, Closure::Callback* callback) noexcept = 0;
};

class InplaceGraphExecutor : public GraphExecutor {
public:
    virtual Closure create_closure() noexcept override;
    virtual int run(GraphVertex* vertex,
                    GraphVertexClosure&& closure) noexcept override;
    virtual int run(ClosureContext* closure,
                    Closure::Callback* callback) noexcept override;

    static InplaceGraphExecutor& instance() noexcept;
};

// 使用线程池进行调度的图执行器
class ThreadPoolGraphExecutor : public GraphExecutor {
public:
    // 初始化
    // worker_num: 线程数
    // queue_capacity: 中转队列容量
    int initialize(size_t worker_num, size_t queue_capacity) noexcept;

    // 停止运行
    void stop() noexcept;

    virtual Closure create_closure() noexcept override;
    virtual int32_t run(GraphVertex* vertex,
        GraphVertexClosure&& closure) noexcept override;
    virtual int32_t run(ClosureContext* closure, Closure::Callback* callback) noexcept override;

private:
    ::babylon::ThreadPoolExecutor _executor;
};

} // anyflow
BABYLON_NAMESPACE_END
