**[[简体中文]](README.zh-cn.md)**

# Babylon

[![CI](https://github.com/baidu/babylon/actions/workflows/ci.yml/badge.svg)](https://github.com/baidu/babylon/actions/workflows/ci.yml)
[![Coverage Status](https://coveralls.io/repos/github/baidu/babylon/badge.svg)](https://coveralls.io/github/baidu/babylon)

Babylon is a foundational library designed to support high-performance C++ server-side development. It provides a wide array of core components focusing on memory and parallelism management. This library is widely applied in scenarios with stringent performance requirements, such as search and recommendation engines, autonomous driving, etc.

## Core Features

- **Efficient application-level memory pool mechanism**
  - Compatible with and extends the [std::pmr::memory_resource](https://en.cppreference.com/w/cpp/memory/memory_resource) mechanism.
  - Integrates and enhances the [google::protobuf::Arena](https://protobuf.dev/reference/cpp/arenas) mechanism.
  - Provides capacity reservation, cleanup, and reconstruction mechanisms when object pools are used in conjunction with memory pools.

- **Modular parallel computing framework**
  - A high-performance automatic parallel framework based on lock-free DAG deduction.
  - Provides a dataflow management scheme derived naturally from the execution flow, ensuring safe data race management in complex computation graph scenarios.
  - Micro-pipeline parallel mechanism offering enhanced parallel capabilities.

- **Core components for parallel development**
  - Wait-free concurrent-safe containers (vector/queue/hash_table/...).
  - Traversable thread-local storage development framework.
  - Extensible synchronization primitives supporting both threads and coroutines (future/mutex/...).

- **Foundational tools for application/framework construction**
  - IOC component development framework.
  - C++ object serialization framework.
  - Zero-copy/zero-allocation asynchronous logging framework.

## Build and Usage

### Supported Platforms and Compilers

- **OS**: Linux
- **CPU**: x86-64/aarch64
- **COMPILER**: gcc/clang

### Bazel

Babylon uses [Bazel](https://bazel.build) for build management and [bzlmod](https://bazel.build/external/module) for dependency management. Given the ongoing transition of the Bazel ecosystem towards bzlmod, Babylon also compatible with the [workspace](https://bazel.build/rules/lib/globals/workspace) dependency management mode.

- [Depend with bazel using bzlmod](example/depend-use-bzlmod)
- [Depend with bazel using workspace](example/depend-use-workspace)

### CMake

Babylon also supports building with [CMake](https://cmake.org) and allows dependency management through [find_package](https://cmake.org/cmake/help/latest/command/find_package.html), [add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html), or [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html).

- [Depend with CMake using FetchContent](example/depend-use-cmake-fetch)
- [Depend with CMake using find_package](example/depend-use-cmake-find)
- [Depend with CMake using add_subdirectory](example/depend-use-cmake-subdir)

## Module Documentation

- [:any](docs/any.en.md)
- [:anyflow](docs/anyflow/README.en.md)
- [:application_context](docs/application_context.en.md)
- [:concurrent](docs/concurrent/README.en.md)
- [:coroutine](docs/coroutine/README.en.md)
- [:executor](docs/executor.en.md)
- [:future](docs/future.en.md)
- [:logging](docs/logging/README.en.md)
  - [Use async logger](example/use-async-logger)
  - [Use with glog](example/use-with-glog)
- [:reusable](docs/reusable/README.en.md)
- [:serialization](docs/serialization.en.md)
- [:time](docs/time.en.md)
- Protobuf [arenastring](docs/arenastring.en.md) patch
- Typical usage with [brpc](https://github.com/apache/brpc)
  - use [:future](docs/future.en.md) with bthread: [example/use-with-bthread](example/use-with-bthread)
  - use [:reusable_memory_resource](docs/reusable/memory_resource.en.md) for rpc server: [example/use-arena-with-brpc](example/use-arena-with-brpc)
  - use [:concurrent_counter](docs/concurrent/counter.en.md) implement bvar: [example/use-counter-with-bvar](example/use-counter-with-bvar)

## Design Philosophy (chinese version only)

- [Extreme optimizations by Baidu C++ engineers (Memory)](https://mp.weixin.qq.com/s?__biz=Mzg5MjU0NTI5OQ==&mid=2247489076&idx=1&sn=748bf716d94d5ed2739ea8a9385cd4a6&chksm=c03d2648f74aaf5e11298cf450c3453a273eb6d2161bc90e411b6d62fa0c1b96a45e411af805&scene=178&cur_album_id=1693053794688761860#rd)
- [Extreme optimizations by Baidu C++ engineers (Concurrency)](https://mp.weixin.qq.com/s/0Ofo8ak7-UXuuOoD0KIHwA)

## How to Contribute

If you encounter any issues or need new features, feel free to create an issue.

If you can solve an issue, you're welcome to submit a PR.

Before sending a PR, please ensure corresponding test cases are included.

## Appreciation

[![Comate PowerSource Initiative Logo](https://comate.baidu.com/images/powersource/powersource-light-en-1.png)](https://comate.baidu.com/en/powerSource)
