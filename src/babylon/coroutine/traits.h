#pragma once

#include "babylon/type_traits.h"

#if __cpp_concepts && __cpp_lib_coroutine

#include <coroutine>

BABYLON_COROUTINE_NAMESPACE_BEGIN

// Check if C(Args...) is not only invocable but also worked as coroutine
template <typename C, typename... Args>
concept Invocable = ::std::invocable<C, Args...> && requires {
  typename ::std::coroutine_traits<::std::invoke_result_t<C, Args...>,
                                   Args...>::promise_type;
};

////////////////////////////////////////////////////////////////////////////////
// Get awaitable type
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// follow the first step in co_await: convert expression to awaitable
template <typename A, typename P>
struct AwaitableFrom {
  using type = A;
};
template <typename A, typename P>
  requires requires {
    ::std::declval<P>().await_transform(::std::declval<A>());
  }
struct AwaitableFrom<A, P> {
  using type =
      decltype(::std::declval<P>().await_transform(::std::declval<A>()));
};
template <typename A, typename P>
using AwaitableType = typename AwaitableFrom<A, P>::type;
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Get awaiter type
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// follow the second step in co_await: convert awaitable to awaiter
template <typename A, typename P>
struct AwaiterFrom {
  using type = AwaitableType<A, P>;
};
template <typename A, typename P>
  requires requires {
    ::std::declval<AwaitableType<A, P>>().operator co_await();
  }
struct AwaiterFrom<A, P> {
  using type =
      decltype(::std::declval<AwaitableType<A, P>>().operator co_await());
};
template <typename A, typename P>
  requires requires {
    operator co_await(::std::declval<AwaitableType<A, P>>());
  }
struct AwaiterFrom<A, P> {
  using type =
      decltype(operator co_await(::std::declval<AwaitableType<A, P>>()));
};
template <typename A, typename P>
using AwaiterType = typename AwaiterFrom<A, P>::type;
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Get result type of a co_await expression
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// function as if: decltype(co_await expression)
//
// Given:
// SomeTaskType awaiter_coroutine() {
//   auto x = co_await awaitee_object;
// }
//
// Then decltype(x) is:
// AwaitableType<SomeTaskType::promise_type, decltype(awaitee_object)>
template <typename A, typename P = void>
using AwaitResultType =
    decltype(::std::declval<AwaiterType<A, P>>().await_resume());
////////////////////////////////////////////////////////////////////////////////

template <typename A, typename P = void>
concept Awaitable = requires { typename AwaitResultType<A, P>; };

BABYLON_COROUTINE_NAMESPACE_END

BABYLON_NAMESPACE_BEGIN
template <typename C, typename... Args>
concept CoroutineInvocable = coroutine::Invocable<C, Args...>;
BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
