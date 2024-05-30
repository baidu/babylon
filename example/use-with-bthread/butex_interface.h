#pragma once

#include "babylon/concurrent/sched_interface.h"

#include "absl/time/clock.h"
#include "bthread/bthread.h"
#include "bthread/butex.h"

BABYLON_NAMESPACE_BEGIN

struct ButexInterface : public SchedInterface {
  inline constexpr static bool futex_need_create() noexcept;
  inline static uint32_t* create_futex() noexcept;
  inline static void destroy_futex(uint32_t* futex) noexcept;

  inline static int futex_wait(uint32_t* futex, uint32_t val,
                               const struct ::timespec* timeout) noexcept;
  inline static int futex_wake_one(uint32_t* futex) noexcept;
  inline static int futex_wake_all(uint32_t* futex) noexcept;

  inline static void usleep(int64_t us) noexcept;
  inline static void yield() noexcept;
};

struct BsleepInterface : public SchedInterface {
  inline static void usleep(int64_t us) noexcept;
};

////////////////////////////////////////////////////////////////////////////////
inline constexpr bool ButexInterface::futex_need_create() noexcept {
  return true;
}

inline uint32_t* ButexInterface::create_futex() noexcept {
  return ::bthread::butex_create_checked<uint32_t>();
}

inline void ButexInterface::destroy_futex(uint32_t* butex) noexcept {
  return ::bthread::butex_destroy(butex);
}

inline int ButexInterface::futex_wait(
    uint32_t* butex, uint32_t val, const struct ::timespec* timeout) noexcept {
  if (timeout == nullptr) {
    return ::bthread::butex_wait(butex, val, nullptr);
  } else {
    auto abstime = ::absl::ToTimespec(::absl::Now() +
                                      ::absl::DurationFromTimespec(*timeout));
    return ::bthread::butex_wait(butex, val, &abstime);
  }
}

inline int ButexInterface::futex_wake_one(uint32_t* butex) noexcept {
  return ::bthread::butex_wake(butex);
}

inline int ButexInterface::futex_wake_all(uint32_t* butex) noexcept {
  return ::bthread::butex_wake_all(butex);
}

inline void ButexInterface::usleep(int64_t us) noexcept {
  ::bthread_usleep(us);
}

inline void ButexInterface::yield() noexcept {
  ::bthread_yield();
}

inline void BsleepInterface::usleep(int64_t us) noexcept {
  ::bthread_usleep(us);
}

BABYLON_NAMESPACE_END
