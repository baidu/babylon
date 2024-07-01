#pragma once

#include "babylon/concurrent/vector.h"

#include <limits>

BABYLON_NAMESPACE_BEGIN

// 附加了版本的值，典型用来解决ABA问题
template <typename T>
struct VersionedValue {
  static_assert(::std::is_same<uint16_t, T>::value ||
                    ::std::is_same<uint32_t, T>::value,
                "support uint16_t or uint32_t only");

  typedef typename ::std::conditional<sizeof(T) == 2, uint32_t, uint64_t>::type
      VersionAndValue;

  inline VersionedValue() noexcept = default;
  inline VersionedValue(VersionAndValue version_and_value) noexcept;

  union {
    VersionAndValue version_and_value;
    struct {
      T value;
      T version;
    };
  };
};

// 唯一标识分配器
// 原理上持续进行std::atomic::fetch_add即可快速获得唯一标识
// 不过这个方法只能持续分配更大的值，对于存在释放的场景无法重复利用之前的值
// 仅仅针对唯一标识这个角度这是足够使用的
// 但是在不仅需要唯一标识，而且还需要用唯一标识管理资源时，例如
// thread id -> thread local
// task id -> task data
// object id -> thread cache
// 如果采用无限增长的唯一标识，则无法使用数组寻址等高效方式进行映射
//
// 原理上和bthread中的base::ResourceId机制类似
// 但是一方面brpc中相关机制不够通用，相似机制存在多套
// 比如baidu::rpc::SocketId封装了版本机制解决ABA
// 比如bvar::detail::AgentGroup因为结合线程局部存储，单独又实现一次
//
// 因此这里单独分离出IdAllocator层，希望可以统一支持所有场景，主要区别为
// 1、内置版本机制解决ABA问题，可以用来异步资源寻址，如SocketId的场景
// 2、解耦了ID分配和存储管理，可以支持AgentGroup的场景
// 3、提供快速已分配ID的遍历接口，支持更高效的汇聚操作
// 4、通过全无锁改造去除了对thread local的依赖，不再强制单例使用
template <typename T>
class IdAllocator {
 public:
  // 可默认构造和移动，不可拷贝
  inline IdAllocator() noexcept = default;
  inline IdAllocator(IdAllocator&&) noexcept = default;
  inline IdAllocator(const IdAllocator&) = delete;
  inline IdAllocator& operator=(IdAllocator&&) noexcept = default;
  inline IdAllocator& operator=(const IdAllocator&) = delete;

  // 分配和释放接口
  VersionedValue<T> allocate();
  void deallocate(VersionedValue<T> id);

  // 分配过的最后一个id + 1，即[0, end)表示了所有曾经分配过的id范围
  inline T end() const noexcept;

  // 遍历当前已分配，未释放的标识，遍历时不提供版本部分，只能取到值
  // 结果通过多次回调callback(begin_id_value, end_id_value)，反馈给调用者
  // 入参应理解为半闭半开区间[begin_id_value,
  // end_id_value)，和典型的iterator体系相同 例如当前已分配未释放的标识为0, 1,
  // 3, 5，则可能会依次调用 callback(0, 2) callback(3, 4) callback(5, 6)
  template <typename C, typename = typename ::std::enable_if<
                            IsInvocable<C, T, T>::value>::type>
  inline void for_each(C&& callback) const;

 private:
  static constexpr T FREE_LIST_TAIL {::std::numeric_limits<T>::max()};
  static constexpr T ACTIVE_FLAG {FREE_LIST_TAIL - 1};

  inline ::std::atomic<T>& next_value() noexcept;
  inline const ::std::atomic<T>& next_value() const noexcept;
  inline ::std::atomic<VersionedValue<T>>& free_head() noexcept;

  T _next_value {0};
  VersionedValue<T> _free_head {FREE_LIST_TAIL};
  ConcurrentVector<::std::atomic<T>, 128> _free_next_value;
};

// 获取当前所在线程的唯一标识
// 原理上syscall
// __NR_gettid/pthread_self/std::this_thread::get_id都可以获取到唯一标识
// 但是ThreadId的实现基于IdAllocator，可以提供【尽量小】且【尽量连续】的编号能力
class ThreadId {
 public:
  // 获取当前线程的标识
  // 支持最多活跃65534个线程，对于合理的服务端程序设计已经足够
  // T: 每个类型有自己的IdAllocator，在线程局部存储设计中可以用来隔离不同类型
  //    避免有些类型只在部分线程使用的情况下，遍历时增加无效的线程
  template <typename T = void>
  static VersionedValue<uint16_t> current_thread_id() noexcept;

  // 创建过的最大一个thread id + 1，即[0, end)表示了所有曾经出现过的线程
  // 典型可以实现遍历所有线程局部存储
  template <typename T = void>
  inline static uint16_t end() noexcept;

  // 遍历当前所有活跃线程，连续取得他们ThreadId的低位value部分
  // 通过多次调用callback(begin_id_value, end_id_value)，反馈给调用者
  // 对于传入callback的输入，应该理解为半闭半开区间[begin_id_value,
  // end_id_value) 例如当前活跃线程位0, 1, 3, 5，则可能会依次调用 callback(0, 2)
  // callback(3, 4)
  // callback(5, 6)
  template <typename T = void, typename C,
            typename = typename ::std::enable_if<
                IsInvocable<C, uint16_t, uint16_t>::value>::type>
  static void for_each(C&& callback);

 private:
  // 内部类型，用thread local持有，使用者不应尝试构造
  inline ThreadId(IdAllocator<uint16_t>& allocator);
  ThreadId(ThreadId&&) = delete;
  ThreadId(const ThreadId&) = delete;
  ThreadId& operator=(ThreadId&&) = delete;
  ThreadId& operator=(const ThreadId&) = delete;
  inline ~ThreadId() noexcept;

  IdAllocator<uint16_t>& _allocator;
  VersionedValue<uint16_t> _value;
};

BABYLON_NAMESPACE_END

#include "babylon/concurrent/id_allocator.hpp"
