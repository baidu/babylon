#pragma once

#include "babylon/reusable/traits.h"

#include <mutex>

BABYLON_NAMESPACE_BEGIN

// 重建实例的访问器
template <typename T>
class ReusableAccessor {
 public:
  inline ReusableAccessor() noexcept = default;
  inline ReusableAccessor(ReusableAccessor&&) noexcept = default;
  inline ReusableAccessor(const ReusableAccessor&) noexcept = default;
  inline ReusableAccessor& operator=(ReusableAccessor&&) noexcept = default;
  inline ReusableAccessor& operator=(const ReusableAccessor&) noexcept =
      default;
  inline ~ReusableAccessor() noexcept = default;

  inline ReusableAccessor(T** instance) noexcept;
  inline T* operator->() const noexcept;
  inline T* get() const noexcept;
  inline T& operator*() const noexcept;
  inline operator bool() const noexcept;

 private:
  // 因为可能会重建实例，这里保存指针的指针
  T** _instance {nullptr};
};

// 通用管理一批可重用实例的生命周期
template <typename R>
class ReusableManager {
 public:
  ReusableManager() = default;
  ReusableManager(ReusableManager&&) = default;
  ReusableManager(const ReusableManager&) = delete;
  ReusableManager& operator=(ReusableManager&&) = default;
  ReusableManager& operator=(const ReusableManager&) = delete;
  ~ReusableManager() noexcept = default;

  // 获取内置的内存池实例，用于相应参数设置
  // 需要注意，一般内存池参数仅能够在使用前进行设置
  // 一旦进行过create_object动作，则不能再进行修改
  R& resource() noexcept;

  // 持续逻辑清空并反复使用同一个实例，可能造成内存空洞碎片
  // 需要触发重建来解决，目前只有简单的重建策略，即
  // 每经历指定次数的逻辑清空，就会重建一次
  void set_recreate_interval(size_t interval) noexcept;

  // 创建一个可重用实例，并获取其访问器
  // 可重用实例会随clear动作进行逻辑清空以及重建
  // 其中重建会引起实际实例地址变化，但是访问器会持续跟踪变换
  // 并保证持续能够返回当前正确的实例地址
  //
  // 为了简化实现，创建操作本身底层有锁同步，需要在非关键路径执行
  // 比如程序启动时，每个处理线程先创建实例并获取到访问器
  // 在后续真实运行时只要通过访问器获取实例即可
  template <typename T, typename... Args>
  ReusableAccessor<T> create_object(Args&&... args) noexcept;

  // 特殊情况下，实例的首次创建可能无法通过uses-allocator
  // construction协议自动完成 支持传入自定义函数来实现
  //
  // 例如通过反射机制使用基类统一管理Protobuf Message的情况
  // 由于T都是基类Message，需要使用反射接口才能创建实际的子类
  // 但是子类创建完成后，就可以将反射信息记录到ReusableTraits机制的AllocationMetadata中
  // 后续的重建就可以依照协议完成了
  template <typename T, typename C,
            typename = typename ::std::enable_if<
                ::std::is_same<T*, ::std::invoke_result_t<C, R&>>::value>::type>
  ReusableAccessor<T> create_object(C&& creator) noexcept;

  // 对所有create_object创建的实例统计执行逻辑清空
  // 并周期性进行重建，以便保持内存尽量复用且持续地连续
  //
  // 清空和重建都需要在实例不被使用时进行，典型在一次业务处理完毕
  // 比如一次rpc结束，一帧数据处理完成等时间点执行
  // 对一次业务流程使用到的可重用实例统一进行重置
  void clear() noexcept;

 private:
  class ReusableUnit {
   public:
    virtual ~ReusableUnit() noexcept = default;

    virtual void clear(R& resource) noexcept = 0;
    virtual void update() noexcept = 0;
    virtual void recreate(R& recreate) noexcept = 0;
  };

  template <typename T>
  class TypedReusableUnit : public ReusableUnit {
   public:
    inline TypedReusableUnit(T* instance) noexcept;

    inline ReusableAccessor<T> accessor() noexcept;

    virtual void clear(R& resource) noexcept override;
    virtual void update() noexcept override;
    virtual void recreate(R& resource) noexcept override;

   private:
    T* _instance;
    Reuse::AllocationMetadata<T> _meta;
  };

  template <typename T>
  ReusableAccessor<T> register_object(T* instance) noexcept;

  R _resource;

  ::std::mutex _mutex;
  ::std::vector<::std::unique_ptr<ReusableUnit>> _units;

  size_t _clear_times {0};
  size_t _recreate_interval {1000};
};

using SwissManager = ReusableManager<SwissMemoryResource>;

BABYLON_NAMESPACE_END

#include "babylon/reusable/manager.hpp"
