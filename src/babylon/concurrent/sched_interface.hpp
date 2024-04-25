#pragma once

#include "babylon/concurrent/sched_interface.h"

#include BABYLON_EXTERNAL(absl/base/attributes.h)   // ABSL_ATTRIBUTE_ALWAYS_INLINE

#include <linux/futex.h>            // FUTEX_*
#include <sched.h>                  // ::sched_yield
#include <sys/syscall.h>            // __NR_futex
#include <unistd.h>                 // ::syscall
#include <utility>                  // std::swap

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// SchedIterface begin
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline constexpr bool SchedInterface::futex_need_create() noexcept {
    return false;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline uint32_t* SchedInterface::create_futex() noexcept {
    return new uint32_t;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void SchedInterface::destroy_futex(uint32_t* futex) noexcept {
    delete futex;
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int SchedInterface::futex_wait(
        uint32_t* futex, uint32_t val,
        const struct ::timespec* timeout) noexcept {
    return ::syscall(__NR_futex, futex,
                     (FUTEX_WAIT | FUTEX_PRIVATE_FLAG), val, timeout);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int SchedInterface::futex_wake_one(uint32_t* futex) noexcept {
    return ::syscall(__NR_futex, futex, (FUTEX_WAKE | FUTEX_PRIVATE_FLAG), 1);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int SchedInterface::futex_wake_all(uint32_t* futex) noexcept {
    return ::syscall(__NR_futex, futex,
                     (FUTEX_WAKE | FUTEX_PRIVATE_FLAG), INT32_MAX);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void SchedInterface::usleep(useconds_t us) noexcept {
    ::usleep(us);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE
inline void SchedInterface::yield() noexcept {
    ::sched_yield();
}
// SchedInterface end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Futex<S, !S::futex_need_create> begin
template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<!S::futex_need_create()>::type>::Futex(
        uint32_t value) noexcept : _value(value) {}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline ::std::atomic<uint32_t>&
Futex<S, typename ::std::enable_if<!S::futex_need_create()>::type>::value() noexcept {
    return reinterpret_cast<::std::atomic<uint32_t>&>(_value);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline const ::std::atomic<uint32_t>&
Futex<S, typename ::std::enable_if<
        !S::futex_need_create()>::type>::value() const noexcept {
    return reinterpret_cast<const ::std::atomic<uint32_t>&>(_value);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int Futex<S, typename ::std::enable_if<!S::futex_need_create()>::type>::wait(
        uint32_t val, const struct ::timespec* timeout) noexcept {
    return S::futex_wait(&_value, val, timeout);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int Futex<S, typename ::std::enable_if<
        !S::futex_need_create()>::type>::wake_one() noexcept {
    return S::futex_wake_one(&_value);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int Futex<S, typename ::std::enable_if<
        !S::futex_need_create()>::type>::wake_all() noexcept {
    return S::futex_wake_all(&_value);
}
// Futex<S, !S::futex_need_create> end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Futex<S, S::futex_need_create> begin
template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<
    S::futex_need_create()>::type>::Futex() noexcept :
        _value(S::create_futex()) {}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>::Futex(
        Futex&& other) noexcept : Futex() {
    ::std::swap(_value, other._value);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>::Futex(
        const Futex& other) noexcept : Futex() {
    *_value = *other._value;
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>&
Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>::operator=(
        Futex&& other) noexcept {
    ::std::swap(_value, other._value);
    return *this;
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>&
Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>::operator=(
        const Futex& other) noexcept {
    *_value = *other._value;
    return *this;
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<
        S::futex_need_create()>::type>::~Futex() noexcept {
    S::destroy_futex(_value);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>::Futex(
        uint32_t value) noexcept : Futex() {
    *_value = value;
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline ::std::atomic<uint32_t>&
Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>::value() noexcept {
    return *reinterpret_cast<::std::atomic<uint32_t>*>(_value);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline const ::std::atomic<uint32_t>&
Futex<S, typename ::std::enable_if<
        S::futex_need_create()>::type>::value() const noexcept {
    return *reinterpret_cast<const ::std::atomic<uint32_t>*>(_value);
}

template <typename S>
ABSL_ATTRIBUTE_ALWAYS_INLINE
inline int Futex<S, typename ::std::enable_if<S::futex_need_create()>::type>::wait(
        uint32_t val, const struct ::timespec* timeout) noexcept {
    return S::futex_wait(_value, val, timeout);
}

template <typename S>
inline int Futex<S, typename ::std::enable_if<
        S::futex_need_create()>::type>::wake_one() noexcept {
    return S::futex_wake_one(_value);
}

template <typename S>
inline int Futex<S, typename ::std::enable_if<
        S::futex_need_create()>::type>::wake_all() noexcept {
    return S::futex_wake_all(_value);
}
// Futex<S, !S::futex_need_create> begin
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
