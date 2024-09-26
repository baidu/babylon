#pragma once

#include "babylon/type_traits.h"

#if __cpp_concepts && __cpp_lib_coroutine

#include <coroutine>

BABYLON_NAMESPACE_BEGIN

// Check if a callable invocation C(Args...) is a coroutine
template <typename C, typename... Args>
concept CoroutineInvocable =
    requires {
      typename ::std::coroutine_traits<::std::invoke_result_t<C, Args...>,
                                       Args...>::promise_type;
    };

////////////////////////////////////////////////////////////////////////////////
// Get awaitable type
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// follow the first step in co_await: convert expression to awaitable
template <typename P, typename A>
struct CoroutineAwaitableFrom {
  using type = A;
};
template <typename P, typename A>
  requires requires {
             ::std::declval<P>().await_transform(::std::declval<A>());
           }
struct CoroutineAwaitableFrom<P, A> {
  using type =
      decltype(::std::declval<P>().await_transform(::std::declval<A>()));
};
template <typename P, typename A>
using CoroutineAwaitableType = typename CoroutineAwaitableFrom<P, A>::type;
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Get awaiter type
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// follow the second step in co_await: convert awaitable to awaiter
template <typename P, typename A>
struct CoroutineAwaiterFrom {
  using type = CoroutineAwaitableType<P, A>;
};
template <typename P, typename A>
  requires requires {
             ::std::declval<CoroutineAwaitableType<P, A>>().operator co_await();
           }
struct CoroutineAwaiterFrom<P, A> {
  using type = decltype(::std::declval<CoroutineAwaitableType<P, A>>().
                        operator co_await());
};
template <typename P, typename A>
  requires requires {
             operator co_await(::std::declval<CoroutineAwaitableType<P, A>>());
           }
struct CoroutineAwaiterFrom<P, A> {
  using type = decltype(operator co_await(
      ::std::declval<CoroutineAwaitableType<P, A>>()));
};
template <typename P, typename A>
using CoroutineAwaiterType = typename CoroutineAwaiterFrom<P, A>::type;
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
// CoroutineAwaitableType<SomeTaskType::promise_type, decltype(awaitee_object)>
template <typename P, typename A>
using CoroutineAwaitResultType =
    decltype(::std::declval<CoroutineAwaiterType<P, A>>().await_resume());
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
