#pragma once

#include "babylon/string_view.h"    // babylon::StringView

#include <chrono>                   // std::chrono::seconds
#include <functional>               // std::function
#include <future>                   // std::promise
#include <thread>                   // std::thread
#include <unordered_map>            // std::unordered_map

BABYLON_NAMESPACE_BEGIN

// 用于将程序虚存锁定到内存中禁止换页
// 典型用法为在启动阶段执行
// MemoryLocker::instance().start()
// 即可将虚存空间中所有关联到文件实体区域进行锁定
// 类似mlockall(MCL_CURRENT | MCL_FUTURE)
// 区别是不会对匿名段产生影响
class MemoryLocker {
public:
    using Filter = ::std::function<bool(StringView)>;

    // 不可拷贝和移动
    MemoryLocker() = default;
    MemoryLocker(MemoryLocker&&) = delete;
    MemoryLocker(const MemoryLocker&) = delete;
    MemoryLocker& operator=(MemoryLocker&&) = delete;
    MemoryLocker& operator=(const MemoryLocker&) = delete;
    
    // 析构时会内置stop
    ~MemoryLocker() noexcept;

    // 设置检测周期和文件过滤器
    // 后台线程每隔check_interval进行一次检测，在变更时尝试调整锁定
    // 检测时只对filter返回false的文件执行锁定，可以过滤临时词典等非常驻文件
    MemoryLocker& set_check_interval(std::chrono::seconds interval) noexcept;
    MemoryLocker& set_filter(const Filter& filter) noexcept;

    // 启动和停止
    int start() noexcept;
    int stop() noexcept;

    // 后台线程每完成一次检测，round + 1
    // locked_bytes为当前成功锁定内存总量
    // last_errno表示最近一次mlock失败的原因
    size_t round() const noexcept;
    size_t locked_bytes() const noexcept;
    int last_errno() const noexcept;

    // 一般锁定器进程级一个足够
    static MemoryLocker& instance() noexcept;

private:
    static bool filter_none(StringView) noexcept;

    void check_and_lock() noexcept;
    void lock_regions(::std::unordered_map<const void*, size_t>&& regions) noexcept;
    void unlock_regions() noexcept;

    Filter _filter {filter_none};
    ::std::chrono::seconds _check_interval {60};

    ::std::thread _thread;
    ::std::promise<void> _stoped;

    ::std::unordered_map<const void*, size_t> _locked_regions;
    size_t _round {0};
    size_t _locked_bytes {0};
    int _last_errno {0};
};

BABYLON_NAMESPACE_END
