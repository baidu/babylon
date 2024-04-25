#pragma once

#include "babylon/environment.h"
#include "babylon/concurrent/sched_interface.h"  // Futex
#include "babylon/type_traits.h"                 // IsInvocable

#include <atomic>       // std::atomic

BABYLON_NAMESPACE_BEGIN

// 基于RingBuffer实现的MPMC队列，核心特性
// 1. 当队列未满时, push操作是wait-free的
// 2. 当队列非空时, pop操作是wait-free的
// 3. 当push或者pop从阻塞中被唤醒后，后续操作是wait-free的
template <typename T, typename S = SchedInterface>
class ConcurrentBoundedQueue {
private:
    // 采用{uint16_t waiters, uint16_t version}打包存储到一个futex中
    class SlotFutex {
    public:
        // 读取版本号部分
        inline uint16_t version(::std::memory_order order) const noexcept;
        // 等待版本就绪
        template <bool USE_FUTEX_WAIT>
        inline void wait_until_reach_expected_version(
                uint16_t expected_version, const struct ::timespec* timeout,
                ::std::memory_order order) noexcept;
        // 推进版本号
        inline void set_version(uint16_t version,
                                ::std::memory_order order) noexcept;
        // 唤醒等待者，和推进版本号之间需要建立seq_cst关系
        inline void wakeup_waiters(uint16_t current_version) noexcept;
        // 推进版本号 && 唤醒等待者
        inline void set_version_and_wakeup_waiters(
                uint16_t next_version) noexcept;
        // 重置到初始状态
        inline void reset() noexcept;

        // 辅助在TSAN模式标注用atomic_thread_fence实现的内存序控制
        inline void mark_tsan_acquire() noexcept;
        inline void mark_tsan_release() noexcept;

    private:
        void block_until_reach_expected_version_slow(
                uint32_t current_version_and_waiters,
                uint16_t expected_version,
                const struct ::timespec* timeout,
                ::std::memory_order order) noexcept;
        void spin_until_reach_expected_version_slow(
                uint32_t current_version_and_waiters,
                uint16_t expected_version,
                const struct ::timespec* timeout,
                ::std::memory_order order) noexcept;

        Futex<S> _futex {0};
    };

    struct alignas(BABYLON_CACHELINE_SIZE) Slot {
        T value;
        SlotFutex futex;
    };

public:
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

    // 可以默认构造和移动，不可拷贝
    // 默认构造队列容量为1
    ConcurrentBoundedQueue() noexcept = default;
    ConcurrentBoundedQueue(ConcurrentBoundedQueue&& other) noexcept;
    ConcurrentBoundedQueue(const ConcurrentBoundedQueue&) = delete;
    ConcurrentBoundedQueue& operator=(ConcurrentBoundedQueue&& other) noexcept;
    ConcurrentBoundedQueue& operator=(const ConcurrentBoundedQueue&) = delete;

    // 按照容量不小于min_capacity构造
    // 为了方便版本判定实际容量将被向上取整到2^n
    ConcurrentBoundedQueue(size_t min_capacity) noexcept;

    // 获取当前容量
    inline size_t capacity() const noexcept;

    // 获取当前队列内待消费元素的**大概数目**
    // 出于性能考虑，并不会和实际操作动作进行同步
    // 计算中包含了**进行中但尚未完成**的push和pop操作
    inline size_t size() const noexcept;

    // 调整队列容量，并重置队列到全空状态
    // 非线程安全，不能与push/pop等操作并发执行
    // return: 调整后的队列容量
    size_t reserve_and_clear(size_t min_capacity);

    // 单元素入队
    // 默认版本push = push<CONCURRENT = true, USE_FUTEX_WAIT = true, USE_FUTEX_WAKE = true>
    // C为回调函数，在获得入队空位后会按照C(T& value)调用，C可以无竞争操作value，在C返回后value被发布
    // CONCURRENT: 是否有其他push可能并发，无并发时底层序号推进将采用非原子操作完成，提升性能
    // USE_FUTEX_WAIT: 是否使用futex_wait阻塞等待，否则采用自旋等待
    //                 USE_FUTEX_WAIT要求相应的pop操作采用USE_FUTEX_WAKE，否则可能无法唤醒
    //                 而USE_FUTEX_WAKE需要无条件引入强内存屏障，有一定性能损失
    //                 虽不多见，但是在pop侧高并发场景下，可以通过省去USE_FUTEX_WAKE来提升性能
    // USE_FUTEX_WAKE: 是否尝试futex_wake唤醒消费者，否则只推进版本
    //                 取决于对应的pop操作是否启用USE_FUTEX_WAIT
    template <typename U, typename ::std::enable_if<
        ::std::is_assignable<T&, U>::value, int>::type = 0>
    inline void push(U&& value);
    template <typename C, typename = typename ::std::enable_if<
        IsInvocable<C, T&>::value>::type>
    inline void push(C&& callback);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
              typename U, typename ::std::enable_if<
                  ::std::is_assignable<T&, U>::value, int>::type = 0>
    inline void push(U&& value);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
             typename C, typename = typename ::std::enable_if<
                 IsInvocable<C, T&>::value>::type>
    inline void push(C&& callback);

    // 单元素入队，非阻塞版本
    // 默认版本try_push = try_push<CONCURRENT = true, USE_FUTEX_WAKE = true>
    // C为回调函数，在获得入队空位后会按照C(T& value)调用
    // C可以无竞争操作value，在C返回后value被发布
    // CONCURRENT: 同pop
    // USE_FUTEX_WAKE: 同pop
    template <typename U, typename ::std::enable_if<
        ::std::is_assignable<T&, U>::value, int>::type = 0>
    inline bool try_push(U&& value) noexcept;
    template <typename C, typename = typename ::std::enable_if<
        IsInvocable<C, T&>::value>::type>
    inline bool try_push(C&& callback) noexcept;
    template <bool CONCURRENT, bool USE_FUTEX_WAKE, typename U,
              typename ::std::enable_if<
                  ::std::is_assignable<T&, U>::value, int>::type = 0>
    inline bool try_push(U&& value) noexcept;
    template <bool CONCURRENT, bool USE_FUTEX_WAKE,
             typename C, typename = typename ::std::enable_if<
                 IsInvocable<C, T&>::value>::type>
    inline bool try_push(C&& callback) noexcept;

    // 批量入队
    // 默认版本push_n = push_n<CONCURRENT = true, USE_FUTEX_WAIT = true, USE_FUTEX_WAKE = true>
    // C为回调函数，在获得入队空位后会按照C(iter begin, iter end)调用，区间为[begin, end)语义，C可以无竞争操作区间
    // 在C返回后整个区间被发布，一次push_n调用中C可能被多次调用，但是多次的区域总量一定等于push_n的数目
    // 使用者**不应该**假定每次调用传入的begin和end间的距离
    // 模板参数含义与push相同
    template <typename IT>
    inline void push_n(IT begin, IT end);
    template <typename C, typename = typename ::std::enable_if<
        IsInvocable<C, Iterator, Iterator>::value>::type>
    inline void push_n(C&& callback, size_t num);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, typename IT>
    inline void push_n(IT begin, IT end);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
        typename C, typename = typename ::std::enable_if<
            IsInvocable<C, Iterator, Iterator>::value>::type>
    inline void push_n(C&& callback, size_t num);

    // 批量入队，非阻塞版本
    // 和push_n的主要区别是，在队满的情况下，不进入阻塞等待状态，而是提前返回
    // 模板参数含义与push相同，由于非阻塞，不具有USE_FUTEX_WAIT参数
    // return: 实际入队元素的数目，可能小于num
    template <bool CONCURRENT, bool USE_FUTEX_WAKE,
             typename C, typename = typename ::std::enable_if<
                 IsInvocable<C, Iterator, Iterator>::value>::type>
    inline size_t try_push_n(C&& callback, size_t num);

    // 批量入队，带补偿版本
    // 另一个push_n的非阻塞版本，在队满的情况下，不会提前返回而是转化为消费者来执行补偿的出队动作
    // 直到最终腾出空间完成足量的入队操作为止
    // 由于存在操作队列两端的可能，一定是有并发不等待的，去掉了全部模板条件参数
    // RC为补偿函数，在队满的情况下，会转而进行补偿出队操作，出队操作采用RC作为回调函数
    template <typename C, typename RC>
    inline void push_n(C&& callback, RC&& reverse_callback, size_t num);

    // 单元素出队
    // 默认版本pop = pop<CONCURRENT = true, USE_FUTEX_WAIT = true, USE_FUTEX_WAKE = true>
    // C为回调函数，在获得出队元素后会按照C(T& value)调用，C可以无竞争操作value
    // 比如典型可以拷贝或者移动到所需空间中在C返回后value被消费，不再继续使用
    // CONCURRENT: 是否有其他pop可能并发，无并发时底层序号推进将采用非原子操作完成，提升性能
    // USE_FUTEX_WAIT: 是否使用futex_wait阻塞等待，否则采用自旋等待
    //                 USE_FUTEX_WAIT要求相应的push操作采用USE_FUTEX_WAKE，否则可能无法唤醒
    //                 而USE_FUTEX_WAKE需要无条件引入强内存屏障，有一定性能损失
    //                 在push侧高并发，pop侧可接受一定唤醒延迟的情况下，可以用来提升push侧的性能
    // USE_FUTEX_WAKE: 是否尝试futex_wake唤醒生产者，否则只推进版本
    //                 取决于对应的push操作是否启用USE_FUTEX_WAIT
    inline void pop(T& value);
    template <typename C, typename = typename ::std::enable_if<
        IsInvocable<C, T&>::value>::type>
    inline void pop(C&& callback);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE>
    inline void pop(T* value);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE>
    inline void pop(T& value);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
             typename C, typename = typename ::std::enable_if<
                 IsInvocable<C, T&>::value>::type>
    inline void pop(C&& callback);

    // 单元素出队，非阻塞版本
    // 默认版本try_pop = try_pop<CONCURRENT = true, USE_FUTEX_WAKE = true>
    // C为回调函数，在获得出队元素后会按照C(T& value)调用，C可以无竞争操作value
    // 比如典型可以拷贝或者移动到所需空间中在C返回后value被消费，不再继续使用
    // CONCURRENT: 同pop
    // USE_FUTEX_WAKE: 同pop
    inline bool try_pop(T& value) noexcept;
    template <typename C, typename = typename ::std::enable_if<
        IsInvocable<C, T&>::value>::type>
    inline bool try_pop(C&& callback) noexcept;
    template <bool CONCURRENT, bool USE_FUTEX_WAKE>
    inline bool try_pop(T& value) noexcept;
    template <bool CONCURRENT, bool USE_FUTEX_WAKE,
             typename C, typename = typename ::std::enable_if<
                 IsInvocable<C, T&>::value>::type>
    inline bool try_pop(C&& callback) noexcept;

    // 批量出队
    // 默认版本pop_n = pop_n<CONCURRENT = true, USE_FUTEX_WAIT = true, USE_FUTEX_WAKE = true>
    // C为回调函数，在获得出队元素后会按照C(iter begin, iter end)调用，区间为[begin, end)语义，C可以无竞争操作区间
    // 在C返回后整个区间被消费，一次pop_n调用中C可能被多次调用，但是多次的区域总量一定等于pop_n的数目
    // 使用者**不应该**假定每次调用传入的begin和end间的距离
    // 模板参数含义与pop相同
    template <typename IT>
    inline void pop_n(IT begin, IT end);
    template <typename C, typename = typename ::std::enable_if<IsInvocable<
        C, Iterator, Iterator>::value>::type>
    inline void pop_n(C&& callback, size_t num);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
              typename IT>
    inline void pop_n(IT begin, IT end);
    template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
              typename C, typename = typename ::std::enable_if<IsInvocable<
                  C, Iterator, Iterator>::value>::type>
    inline void pop_n(C&& callback, size_t num);

    // 批量出队，非阻塞版本
    // 和pop_n的主要区别是，在队空的情况下，不进入阻塞等待状态，而是提前返回
    // 模板参数含义与push相同，由于非阻塞，不具有USE_FUTEX_WAIT参数
    // return: 实际出队元素的数目，可能小于num
    template <bool CONCURRENT, bool USE_FUTEX_WAKE,
             typename C, typename = typename ::std::enable_if<
                 IsInvocable<C, Iterator, Iterator>::value>::type>
    inline size_t try_pop_n(C&& callback, size_t num);

    // 批量出队，带超时阻塞版本
    // 和pop_n的主要区别是，在元素不足时，等待最多timeout时间
    // 且只能独占使用，因此不具有CONCURRENT和USE_FUTEX_WAIT参数
    // return: 实际出队元素的数目，可能小于num
    template <bool USE_FUTEX_WAKE,
              typename C, typename = typename ::std::enable_if<
                  IsInvocable<C, Iterator, Iterator>::value>::type>
    inline size_t try_pop_n_exclusively_until(
            C&& callback, size_t num,
            const struct ::timespec* timeout) noexcept;

    // 批量出队，带补偿版本
    // 另一个pop_n的非阻塞版本，在队空的情况下，不会提前返回而是转化为生产者来执行补偿的入队动作
    // 直到最终满足足量的出队操作为止
    // 由于存在操作队列两端的可能，一定是有并发非阻塞的，去掉了全部模板条件参数
    // RC为补偿函数，在队空的情况下，会转而进行补偿入队操作，入队操作采用RC作为回调函数
    template <typename C, typename RC>
    inline void pop_n(C&& callback, RC&& reverse_callback, size_t num);

    // 清理未消费数据，之后可以重新使用队列
    void clear();

    // 交换两个队列
    void swap(ConcurrentBoundedQueue& other) noexcept;

    inline void pop(T* value);

private:
    class SlotVector {
    public:
        SlotVector() noexcept = default;
        SlotVector(SlotVector&& other) noexcept;
        SlotVector(const SlotVector&) = delete;
        SlotVector& operator=(SlotVector&&) noexcept;
        SlotVector& operator=(const SlotVector&) = delete;
        ~SlotVector() noexcept;

        SlotVector(size_t size) noexcept;

        void resize(size_t size) noexcept;

        inline size_t size() const noexcept;

        inline T& value(size_t index) noexcept;
        inline Iterator value_iterator(size_t index) noexcept;
        inline SlotFutex& futex(size_t index) noexcept;

        void swap(SlotVector& other) noexcept;

    private:
        Slot* _slots {nullptr};
        size_t _size {0};
    };

    // index代表队列的逻辑空间[0, ...)，其中后_slot_bits位表示slot
    // slot对应到_slots数组中的一个具体单元
    // 其余位标识epoch，数组每被循环使用一次，epoch增加1
    // 每个epoch的push和pop操作各自具有一个version，对于push
    // version[slot] = 2 * epoch + 1
    // 而对于pop操作
    // version[slot] = 2 * epoch + 2
    // 考虑到实际上同时重叠出现的version规模不会太大
    // 实现时version被压缩到uint16_t内，便于打包存储到slot中
    inline uint16_t push_version_for_index(size_t index) noexcept;
    inline uint16_t pop_version_for_index(size_t index) noexcept;

    template <bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP,
              typename C>
    inline void deal(C&& callback, size_t index) noexcept;
    template <bool CONCURRENT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP,
              typename C>
    inline bool try_deal(C&& callback) noexcept;
    template <bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP,
              typename C>
    inline void deal_n_continuously(C&& callback,
                                    size_t index, size_t num) noexcept;
    template <bool PUSH_OR_POP, typename C, typename RC>
    inline void deal_n_continuously(C&& callback, RC&& reverse_callback,
            size_t index, size_t num) noexcept;
    template <bool CONCURRENT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP,
              typename C>
    inline size_t try_deal_n_continuously(C&& callback,
                                          size_t index, size_t num) noexcept;

    SlotVector _slots {1};
    size_t _slot_mask {0};
    size_t _slot_bits {0};

#if !__clang__ && BABYLON_GCC_VERSION < 50000
    alignas(BABYLON_CACHELINE_SIZE) ::std::atomic<size_t> _next_push_index = ATOMIC_VAR_INIT(0);
    alignas(BABYLON_CACHELINE_SIZE) ::std::atomic<size_t> _next_pop_index = ATOMIC_VAR_INIT(0);
#else // __clang__ || BABYLON_GCC_VERSION >= 50000
    alignas(BABYLON_CACHELINE_SIZE) ::std::atomic<size_t> _next_push_index {0};
    alignas(BABYLON_CACHELINE_SIZE) ::std::atomic<size_t> _next_pop_index {0};
#endif // __clang__ || BABYLON_GCC_VERSION >= 50000
};

BABYLON_NAMESPACE_END

#include "babylon/concurrent/bounded_queue.hpp"
