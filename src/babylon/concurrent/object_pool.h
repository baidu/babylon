#pragma once

#include "babylon/concurrent/bounded_queue.h" // ConcurrentBoundedQueue
#include "babylon/move_only_function.h"       // MoveOnlyFunction

#include <memory> // std::unique_ptr

BABYLON_NAMESPACE_BEGIN

// 对象池，主要用于支持一些复用一些『昂贵』的实例
// 一般『昂贵』体现在两方面
// 1、创建和销毁开销较大，需要通过反复使用来避免
// 2、实例持有的资源很紧缺，需要严格定量
//
// 例如『对用一个实例的TCP连接』或『一次模型推理的执行环境』
// 为了应对两种不同场景，对象池支持严格竞争和自动创建两种模式
template <typename T>
class ObjectPool {
 public:
  class Deleter;

  // 默认构造实例容量为0，且处于严格竞争模式
  ObjectPool() noexcept = default;
  ObjectPool(ObjectPool&&) noexcept = default;
  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(ObjectPool&&) noexcept = default;
  ObjectPool& operator=(const ObjectPool&) = delete;
  ~ObjectPool() noexcept = default;

  // 设定对象池最大容量
  // 严格竞争模式下：需要设置为不小于总量
  //                 否则初始化阶段push过量数据时会因为容量不足卡住
  // 自动创建模式下：会监测当前free object总量
  //                 当超过capacity时后续push会自动销毁过量实例
  void reserve_and_clear(size_t capacity) noexcept;

  // 设置自动创建实例的回调函数，并切换为自动创建模式
  // 在pop操作遇到实例不足的情况时，会通过回调函数来自动创建实例返回
  template <typename C>
  void set_creator(C&& create_callback) noexcept;

  // 设置清理实例的回调函数，默认为空
  // 在实例回到对象池重新利用前，会调用回调函数来重置实例状态
  template <typename C>
  void set_recycler(C&& recycle_callback) noexcept;

  // 从对象池中获取一个可用的实例，返回的智能指针析构时会自动push回池
  // 严格竞争模式下：如果池为空，则会持续等待，直到有实例被释放push回池
  // 自动创建模式下：如果池为空，则会通过set_creator注册的回调函数构造一个
  inline ::std::unique_ptr<T, Deleter> pop() noexcept;

  // 从对象池中获取一个可用的实例，返回的智能指针析构时会自动push回池
  // 如果当前池为空，则会直接返回空指针
  inline ::std::unique_ptr<T, Deleter> try_pop() noexcept;

  // 将一个可用实例放入/放回对象池，实例可以来自对象池，也可以外部预先构造好
  // 严格竞争模式下：因为无法自动创建，需要通过push预先注入实例，对象池才能投入使用
  //                 运行时，也可以继续通过push注入更多实例
  //                 但是要注意不能注入超过容量，否则可能会导致卡住
  // 自动创建模式下：无需预先进行实例注入，一般都是通过自动push回池
  //                 当池中实例超过容量时会自动销毁
  inline void push(::std::unique_ptr<T>&& object) noexcept;
  inline void push(::std::unique_ptr<T, Deleter>&& object) noexcept;

  inline size_t free_object_number() const noexcept;

 private:
  ConcurrentBoundedQueue<::std::unique_ptr<T>> _free_objects;
  size_t _capacity {0};
  MoveOnlyFunction<::std::unique_ptr<T>()> _object_creator;
  MoveOnlyFunction<void(T&)> _object_recycler {[](T&) {}};
};

template <typename T>
class ObjectPool<T>::Deleter {
 public:
  inline Deleter() noexcept = default;
  inline Deleter(Deleter&& other) noexcept;
  Deleter(const Deleter&) = delete;
  inline Deleter& operator=(Deleter&& other) noexcept;
  Deleter& operator=(const Deleter&) = delete;
  inline ~Deleter() noexcept = default;

  inline Deleter(ObjectPool<T>* pool) noexcept;

  inline void operator()(T* ptr) noexcept;

 private:
  ObjectPool<T>* _pool {nullptr};
};

BABYLON_NAMESPACE_END

#include "babylon/concurrent/object_pool.hpp"
