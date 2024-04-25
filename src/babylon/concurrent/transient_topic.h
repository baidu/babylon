#pragma once

#include "babylon/concurrent/sched_interface.h"     // Futex
#include "babylon/concurrent/vector.h"              // ConcurrentTransientTopic

BABYLON_NAMESPACE_BEGIN

template <typename T, typename S = SchedInterface>
class ConcurrentTransientTopic {
private:
    struct Slot;
    using SlotVector = ConcurrentVector<Slot, 128>;

public:
    class Iterator;
    class Consumer;
    class ConstConsumer;
    class ConstConsumeRange;

    // 可默认构造和移动，不可拷贝
    inline ConcurrentTransientTopic() noexcept = default;
    inline ConcurrentTransientTopic(ConcurrentTransientTopic&& other) noexcept;
    inline ConcurrentTransientTopic(
            const ConcurrentTransientTopic&) noexcept = delete;
    inline ConcurrentTransientTopic& operator=(
            ConcurrentTransientTopic&& other) noexcept;
    inline ConcurrentTransientTopic& operator=(
            const ConcurrentTransientTopic&) noexcept = delete;
    inline ~ConcurrentTransientTopic() noexcept = default;

    // 预留至少容纳size个元素的空间
    inline void reserve(size_t size) noexcept;

    template <typename U, typename ::std::enable_if<
        ::std::is_assignable<T&, U>::value, int>::type = 0>
    inline void publish(U&& value) noexcept;
    template <bool CONCURRENT, typename U, typename ::std::enable_if<
        ::std::is_assignable<T&, U>::value, int>::type = 0>
    inline void publish(U&& value) noexcept;

    // 批量发布操作
    // C为回调函数，在获得发布空位后会按照C(iter begin, iter end)调用
    // 区间为[begin, end)语义，C可以无竞争操作区间
    // 在C返回后整个区间被发布，一次publish_n调用中C可能被多次调用
    // 但是多次的区域包含的总量一定等于publish_n的数目
    // 使用者**不应该**假定每次调用传入的begin和end间的距离
    // CONCURRENT: 是否有其他发布操作可能并发
    //             无并发时底层序号推进将采用非原子操作完成，提升性能
    // USE_FUTEX_WAKE: 是否尝试futex_wake唤醒消费者，否则只推进版本
    //                 取决于对应的消费操作是否启用USE_FUTEX_WAIT
    template <typename C,
              typename = typename ::std::enable_if<
                  IsInvocable<C, Iterator, Iterator>::value>::type>
    inline void publish_n(size_t num, C&& callback) noexcept;
    template <bool CONCURRENT, typename C,
              typename = typename ::std::enable_if<
                  IsInvocable<C, Iterator, Iterator>::value>::type>
    inline void publish_n(size_t num, C&& callback) noexcept;

    // 结束全部发布，需要在全部发布均发布后再执行
    // 用于通知消费者发布结束，让其结束处理流程
    inline void close() noexcept;

    // 创建一个消费者，调用多次可以创建多个独立的消费者
    // 每个消费者独立从0开始消费全部数据
    inline Consumer subscribe() noexcept;
    inline ConstConsumer subscribe() const noexcept;

    // 清理已发布数据，重置进度，以便重新使用
    // 不会释放已经填充的数据，后续publish可以复用这些对象
    inline void clear() noexcept;

    class Iterator {
    public:
        using difference_type = ssize_t;
        using value_type = T;
        using pointer  = T*;
        using reference  = T&;
        using iterator_category = ::std::random_access_iterator_tag;

        inline Iterator(Slot* slot) noexcept;
        inline Iterator& operator++() noexcept;
        inline Iterator operator++(int) noexcept;
        inline Iterator operator+(ssize_t offset) const noexcept;
        inline Iterator operator-(ssize_t offset) const noexcept;
        inline bool operator==(Iterator other) const noexcept;
        inline bool operator!=(Iterator other) const noexcept;
        inline bool operator<(Iterator other) const noexcept;
        inline bool operator<=(Iterator other) const noexcept;
        inline bool operator>(Iterator other) const noexcept;
        inline bool operator>=(Iterator other) const noexcept;
        inline T& operator*() const noexcept;
        inline T* operator->() const noexcept;
        inline ssize_t operator-(Iterator other) const noexcept;

    private:
        Slot* _slot {nullptr};
    };

    // 一个消费区段，Consumer.consume(num)返回值
    class ConsumeRange {
    public:
        // 默认构造无效区段
        inline ConsumeRange() noexcept = default;

        // 判断是否是个有效的区段
        // 消费全部结束时，会返回无效区段
        inline operator bool() const noexcept;

        // 区段数据量，无效区段为0
        inline size_t size() const noexcept;

        // 下标获取待消费数据，[0, size)
        inline T& operator[](size_t index) noexcept;
        inline const T& operator[](size_t index) const noexcept;

    private:
        // 内部构造使用，针对queue[begin, end)创建消费区段
        inline ConsumeRange(typename SlotVector::Snapshot snapshot,
                            size_t begin, size_t size) noexcept;

        typename SlotVector::Snapshot _snapshot;
        size_t _begin {0};
        size_t _size {0};

        friend class Consumer;
    };

    // 消费者，通过subscribe创建
    // 使用consume获取发布的数据
    // 当已发布数据量未达到预期时，阻塞等待
    // 消费者之间可以安全并发，但一个消费者自身的consume非线程安全
    class Consumer {
    public:
        // 默认构造无效空实例
        inline Consumer() noexcept = default;

        // 判断是否是个有效实例
        inline operator bool() const noexcept;

        // 已经关闭且数据全部消费完成后，返回nullptr
        // 否则消费一个数据并返回数据指针
        inline T* consume() noexcept;

        // 已经关闭且数据全部消费完成后，返回无效ConsumeRange
        // 否则消费并返回[1, num]个待消费数据构成的ConsumeRange
        // 在返回小于num的情况下，也可以确认全部消费完成
        inline ConsumeRange consume(size_t num) noexcept;

    private:
        inline Consumer(ConcurrentTransientTopic* queue) noexcept;

        ConcurrentTransientTopic* _queue {nullptr};
        size_t _next_consume_index {0};

        friend class ConcurrentTransientTopic;
    };

    class ConstConsumeRange : private ConsumeRange {
    public:
        inline ConstConsumeRange() noexcept = default;

        inline ConstConsumeRange(ConsumeRange other) noexcept;

        using ConsumeRange::operator bool;
        using ConsumeRange::size;

        inline const T& operator[](size_t index) const noexcept;
    };

    class ConstConsumer : private Consumer {
    public:
        inline ConstConsumer() noexcept = default;

        inline ConstConsumer(const Consumer& other) noexcept;

        inline const T* consume() noexcept;
        inline ConstConsumeRange consume(size_t num) noexcept;
    };

private:
    static constexpr uint16_t INITIAL = 0;
    static constexpr uint16_t PUBLISHED = 1;
    static constexpr uint16_t CLOSED = 2;

    class SlotFutex {
    public:
        // 设置状态，需要和之前的发布数据写入操作建立release序
        inline void set_published() noexcept;
        inline void set_closed() noexcept;

        // 唤醒等待者，和设置状态中间需要建立seq_cst序
        inline void wakeup_waiters() noexcept;
        void wakeup_waiters_slow(uint32_t current_status_and_waiters) noexcept;

        // 检测状态，之后使用数据前需要建立acquire序
        inline bool is_published() const noexcept;
        inline bool is_closed() const noexcept;

        // 等待进入PUBLISHED或CLOSED状态
        inline void wait_until_ready() noexcept;
        void wait_until_ready_slow(
                uint32_t current_status_and_waiters) noexcept;

        inline void reset() noexcept;

        // 辅助在TSAN模式标注用atomic_thread_fence实现的内存序控制
        inline void mark_tsan_acquire() noexcept;
        inline void mark_tsan_release() noexcept;

    private:
        Futex<S> _futex {INITIAL};
    };

    struct alignas(BABYLON_CACHELINE_SIZE) Slot {
        T value;
        SlotFutex futex;
    };

    // 可以默认构造和移动，不可拷贝
    // 存储槽位，发布内容和关闭都作为事件存放在这
    // 逻辑上是一个可下标寻址的连续空间
    SlotVector _slots;

    alignas(BABYLON_CACHELINE_SIZE) ::std::atomic<size_t> _next_event_index {0};

    friend class Consumer;
};

BABYLON_NAMESPACE_END

#include "babylon/concurrent/transient_topic.hpp"
