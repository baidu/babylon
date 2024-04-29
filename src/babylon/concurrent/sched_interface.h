#pragma once

#include "babylon/environment.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/optimization.h) // TODO(lijiang01): 等baidu/adu-lab/energon版本推进后移除
// clang-format on

#include "babylon/absl_numeric_bits.h" // TODO(lijiang01): 等baidu/adu-lab/energon版本推进后移除

#include <stdint.h>   // int*_t
#include <sys/time.h> // ::timespec
#include <unistd.h>   // ::useconds_t

#include <atomic>      // std::atomic
#include <type_traits> // std::enable_if

BABYLON_NAMESPACE_BEGIN

// 统一定义一套调度接口封装
// 主要便于统一支持内核态和用户态调度切换机制
struct SchedInterface {
  // 用户态实现一般需要明确创建信号量
  // 而内核态机制一般无需创建，基于对齐地址即可
  constexpr static bool futex_need_create() noexcept;
  inline static uint32_t* create_futex() noexcept;
  inline static void destroy_futex(uint32_t* futex) noexcept;

  // futex同步函数，抽象提供最基础的wait和wake功能
  inline static int futex_wait(uint32_t* futex, uint32_t val,
                               const struct ::timespec* timeout) noexcept;
  inline static int futex_wake_one(uint32_t* futex) noexcept;
  inline static int futex_wake_all(uint32_t* futex) noexcept;

  // 让出指定的一段时间的执行资源
  inline static void usleep(useconds_t us) noexcept;

  // 让出当前的执行资源
  // 但如果没有其他可执行任务会立刻恢复执行
  inline static void yield() noexcept;
};

// 单个futex同步量的简化封装版本，消除futex_need_create带来的不同
template <typename S, typename E = void>
class Futex {
 public:
  inline Futex() noexcept;
  inline Futex(Futex&& other) noexcept;
  inline Futex(const Futex& other) noexcept;
  inline Futex& operator=(Futex&& other) noexcept;
  inline Futex& operator=(const Futex& other) noexcept;
  inline ~Futex() noexcept;

  inline Futex(uint32_t value) noexcept;

  inline ::std::atomic<uint32_t>& value() noexcept;
  inline const ::std::atomic<uint32_t>& value() const noexcept;

  inline int wait(uint32_t val, const struct ::timespec* timeout) noexcept;
  inline int wake_one() noexcept;
  inline int wake_all() noexcept;
};

// 无需创建的情况下，单个futex就是一个uint32_t
template <typename S>
class Futex<S, typename std::enable_if<!S::futex_need_create()>::type> {
 public:
  inline Futex() noexcept = default;
  inline Futex(Futex&& other) noexcept = default;
  inline Futex(const Futex& other) noexcept = default;
  inline Futex& operator=(Futex&& other) noexcept = default;
  inline Futex& operator=(const Futex& other) noexcept = default;
  inline ~Futex() noexcept = default;

  inline Futex(uint32_t value) noexcept;

  inline ::std::atomic<uint32_t>& value() noexcept;
  inline const ::std::atomic<uint32_t>& value() const noexcept;

  inline int wait(uint32_t val, const struct ::timespec* timeout) noexcept;
  inline int wake_one() noexcept;
  inline int wake_all() noexcept;

 private:
  uint32_t _value;
};

// 需要创建的情况
template <typename S>
class Futex<S, typename std::enable_if<S::futex_need_create()>::type> {
 public:
  inline Futex() noexcept;
  inline Futex(Futex&& other) noexcept;
  inline Futex(const Futex& other) noexcept;
  inline Futex& operator=(Futex&& other) noexcept;
  inline Futex& operator=(const Futex& other) noexcept;
  inline ~Futex() noexcept;

  inline Futex(uint32_t value) noexcept;

  inline ::std::atomic<uint32_t>& value() noexcept;
  inline const ::std::atomic<uint32_t>& value() const noexcept;

  inline int wait(uint32_t val, const struct ::timespec* timeout) noexcept;
  inline int wake_one() noexcept;
  inline int wake_all() noexcept;

 private:
  uint32_t* _value;
};

BABYLON_NAMESPACE_END

#include "babylon/concurrent/sched_interface.hpp"
