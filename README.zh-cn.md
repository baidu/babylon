**[[English]](README.en.md)**

# Babylon

[![CI](https://github.com/baidu/babylon/actions/workflows/ci.yml/badge.svg)](https://github.com/baidu/babylon/actions/workflows/ci.yml)
[![Coverage Status](https://coveralls.io/repos/github/baidu/babylon/badge.svg)](https://coveralls.io/github/baidu/babylon)

Babylon是一个用于支持C++高性能服务端开发的基础库，从内存和并行管理角度提供了大量的基础组件。广泛应用在对性能有严苛要求的场景，典型例如搜索推荐引擎，自动驾驶车载计算等场景

## 核心功能

- 高效的应用级内存池机制
  - 兼容并扩展了[std::pmr::memory_resource](https://en.cppreference.com/w/cpp/memory/memory_resource)机制
  - 整合并增强了[google::protobuf::Arena](https://protobuf.dev/reference/cpp/arenas)机制
  - 在对象池结合内存池使用的情况下提供了保留容量清理和重建的机制
- 组件式并行计算框架
  - 基于无锁DAG推导的高性能自动组件并行框架
  - 依照执行流天然生成数据流管理方案，复杂计算图场景下提供安全的数据竞争管理
  - 微流水线并行机制，提供上限更好的并行化能力
- 并行开发基础组件
  - wait-free级别的并发安全容器（vector/queue/hash_table/...）
  - 可遍历的线程缓存开发框架
  - 可扩展支持线程/协程的同步原语（future/mutex/...）
- 应用/框架搭建基础工具
  - IOC组件开发框架
  - C++对象序列化框架
  - 零拷贝/零分配异步日志框架

## 编译并使用

### 支持平台和编译器

- OS: Linux
- CPU: x86-64/aarch64
- COMPILER: gcc/clang

### Bazel

Babylon使用[Bazel](https://bazel.build)进行构建并使用[bzlmod](https://bazel.build/external/module)进行依赖管理，考虑到目前Bazel生态整体处于bzlmod的转换周期，Babylon也依然兼容[workspace](https://bazel.build/rules/lib/globals/workspace)依赖管理模式

- [Depend with bazel use bzlmod](example/depend-use-bzlmod)
- [Depend with bazel use workspace](example/depend-use-workspace)

### CMake

Babylon也支持使用[CMake](https://cmake.org)进行构建，并支持通过[find_package](https://cmake.org/cmake/help/latest/command/find_package.html)、[add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)或[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)进行依赖引入

- [Depend with cmake use FetchContent](example/depend-use-cmake-fetch)
- [Depend with cmake use find_package](example/depend-use-cmake-find)
- [Depend with cmake use add_subdirectory](example/depend-use-cmake-subdir)

## 模块功能文档

- [:any](docs/any.zh-cn.md)
- [:anyflow](docs/anyflow/README.zh-cn.md)
- [:application_context](docs/application_context.zh-cn.md)
- [:concurrent](docs/concurrent/README.zh-cn.md)
- [:coroutine](docs/coroutine/README.zh-cn.md)
- [:executor](docs/executor.zh-cn.md)
- [:future](docs/future.zh-cn.md)
- [:logging](docs/logging/README.zh-cn.md)
  - [Use async logger](example/use-async-logger)
  - [Use with glog](example/use-with-glog)
- [:reusable](docs/reusable/README.zh-cn.md)
- [:serialization](docs/serialization.zh-cn.md)
- [:time](docs/time.zh-cn.md)
- Protobuf [arenastring](docs/arenastring.zh-cn.md) patch
- Typical usage with [brpc](https://github.com/apache/brpc)
  - use [:future](docs/future.zh-cn.md) with bthread: [example/use-with-bthread](example/use-with-bthread)
  - use [:reusable_memory_resource](docs/reusable/memory_resource.zh-cn.md) for rpc server: [example/use-arena-with-brpc](example/use-arena-with-brpc)
  - use [:concurrent_counter](docs/concurrent/counter.zh-cn.md) implement bvar: [example/use-counter-with-bvar](example/use-counter-with-bvar)

## 整体设计思路

- [百度C++工程师的那些极限优化（内存篇）](https://mp.weixin.qq.com/s?__biz=Mzg5MjU0NTI5OQ==&mid=2247489076&idx=1&sn=748bf716d94d5ed2739ea8a9385cd4a6&chksm=c03d2648f74aaf5e11298cf450c3453a273eb6d2161bc90e411b6d62fa0c1b96a45e411af805&scene=178&cur_album_id=1693053794688761860#rd)
- [百度C++工程师的那些极限优化（并发篇）](https://mp.weixin.qq.com/s/0Ofo8ak7-UXuuOoD0KIHwA)

## 如何贡献

如果你遇到问题或需要新功能，欢迎创建issue。

如果你可以解决某个issue, 欢迎发送PR。

发送PR前请确认有对应的单测代码。

## 致谢

[![文心快码源力计划标志](https://comate.baidu.com/images/powersource/powersource-light-zh-1.png)](https://comate.baidu.com/zh/powerSource)
