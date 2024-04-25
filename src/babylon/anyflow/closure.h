#pragma once

#include "babylon/move_only_function.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

class GraphExecutor;
class ClosureContext;
class Closure {
public:
    using Callback = ::babylon::MoveOnlyFunction<void(Closure&&)>;

    // 创建一个closure
    template <typename S>
    inline static Closure create(GraphExecutor& executor) noexcept;
    
    Closure() {}
    
    inline Closure(Closure&& closure) noexcept;

    inline Closure& operator=(Closure&& closure);
    // 是否已经进入终止态
    inline bool finished() const noexcept;
    // 阻塞等待进入终止态，并返回错误码
    inline int32_t get() noexcept;
    // 阻塞等待进入稳态
    inline void wait() noexcept;
    // 进入终止态后可以获得错误码
    inline int32_t error_code() const noexcept;
    // 注册callback
    // 调用后当前Closure对象不再可用
    // 等待finish之后，callback会被调用
    // 等效的Closure会被传入，用于反馈结果，以及进一步跟踪图状态
    template <typename C>
    inline void on_finish(C&& callback) noexcept;

private:
    inline Closure(ClosureContext* context) noexcept;
    inline ClosureContext* context() noexcept;

    ::std::unique_ptr<ClosureContext> _context;
    
    friend class Graph;
    friend class GraphExecutor;
    friend class ClosureContext;
    friend class BthreadGraphExecutor;
};

class GraphData;
class GraphExecutor;
class ClosureContext {
public:
    inline ClosureContext(GraphExecutor& executor) noexcept;
    virtual ~ClosureContext() noexcept;

    ///////////////////////////////////////////////////////////////////////////
    // 通过Closure暴露，主要用于状态获取
    // 是否已经进入终止态
    inline bool finished() const noexcept;
    // 阻塞等待进入终止态，并返回错误码
    inline int32_t get() noexcept;
    // 阻塞等待进入稳态
    inline void wait() noexcept;
    // 进入终止态后可以获得错误码
    inline int32_t error_code() const noexcept;
    // 注册callback
    // 调用后当前Closure对象不再可用
    // 当finish之后，callback会被调用
    // 等效的Closure会被传入，用于反馈结果，以及进一步跟踪图状态
    template <typename C>
    inline void on_finish(C&& callback) noexcept;
    ///////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////
    // 通过Graph进行运行时计数，用于终止态判断
    // 创建时Closure计数多计算了1，用于逐步记入data和vertex
    // 当全部准备就绪后需要调用fire去掉多余的计数
    inline void fire() noexcept;
    // 进入终止态，并设置返回码
    // 返回true表示需要executor异步触发回调
    inline void finish(int32_t error_code) noexcept;
    // 操作待ready的data数，全部data都ready进入终止态
    // 此时可能还有运行中的节点，析构和重用需要进入稳态
    inline void depend_data_add() noexcept;
    // 返回true表示需要executor异步触发回调
    inline void depend_data_sub() noexcept;
    // 操作运行中的节点数，节点数到0进入稳态
    // 如果还没有进入终止态，则强制失败终止
    // 析构时也会先等待先进入稳态，确保没有竞争
    inline void depend_vertex_add() noexcept;
    // 返回true表示需要executor异步触发回调
    inline void depend_vertex_sub() noexcept;
    inline void add_waiting_data(GraphData* data) noexcept;
    inline void all_data_num(size_t num) noexcept;
    ///////////////////////////////////////////////////////////////////////////

    // 运行callback，由executer在异步环境中调用
    // 运行结束后会销毁this和callback
    inline void run(Closure::Callback* callback) noexcept;

protected:
    ///////////////////////////////////////////////////////////////////////////
    // 使用具体mutex实现wait和notify
    virtual void wait_finish() noexcept = 0;
    virtual void notify_finish() noexcept = 0;
    virtual void wait_flush() noexcept = 0;
    virtual void notify_flush() noexcept = 0;
    ///////////////////////////////////////////////////////////////////////////

private:
    // 使用executor执行callback
    // executor准备好执行环境后，会在异步环境调用run
    int invoke(Closure::Callback* callback) noexcept;
    // 标记finish，只有第一次标记成功
    // 标记成功时，如果已经注册了callback，会通过callback返回
    inline bool mark_finished(int32_t error_code, Closure::Callback*& callback) noexcept;
    void log_unfinished_data() const noexcept;

    static Closure::Callback* SEALED_CALLBACK;

    GraphExecutor* _executor;
    ::std::atomic<int64_t> _waiting_vertex_num {1};
    ::std::atomic<int64_t> _waiting_data_num {1};
    ::std::atomic<Closure::Callback*> _callback {nullptr};
    int32_t _error_code {0};

    Closure::Callback* _flush_callback {nullptr};
    ::std::vector<GraphData*> _waiting_data;
    size_t _all_data_num {0};
};

} // anyflow
BABYLON_NAMESPACE_END

#include "babylon/anyflow/closure.hpp"
