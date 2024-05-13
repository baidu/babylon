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

Babylon使用[Bazel](https://bazel.build)进行构建和依赖管理
- 构建`bazel build ...`
- 单测`bazel test ...`
- Asan单测`bazel test --config asan ...`
- Tsan单测`bazel test --config tsan ...`

通过[bzlmod](https://bazel.build/external/module)机制依赖
- 增加仓库注册表
```
# in .bazelrc
common --registry=file://%workspace%/registry
common --registry=https://raw.githubusercontent.com/bazelboost/registry/main
```
- 增加依赖项
```
# in MODULE.bazel
bazel_dep(name = 'babylon', version = '1.1.4')
```
- 精细化使用子模块依赖目标`@babylon//:any`，`@babylon/:concurrent`等，详见[BUILD](BUILD)文件
- 或者使用All in One依赖目标`@babylon//:babylon`

Babylon也支持[CMake](https://cmake.org)进行构建，以及通过[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)进行自动依赖下载
- 环境准备
  - 使用预编译依赖`cmake -Bbuild`
  - 使用自动依赖下载`cmake -Bbuild -DBUILD_DEPS=ON`
- 编译`cmake --build build`
- 单测`ctest --test-dir build`
- CMake编译目前只提供All in One依赖目标`babylon::babylon`

## 模块功能文档

- [:any](docs/any.md)
- [:anyflow](docs/anyflow/index.md)
- [:concurrent](docs/concurrent/index.md)
- [:executor](docs/executor.md)
- [:future](docs/future.md)
- [:logging](docs/logging.md)
- [:reusable](docs/reusable/index.md)
- [:serialization](docs/serialization.md)
- [:time](docs/time.md)
- Protobuf [arenastring](docs/arenastring.md) patch

## 整体设计思路

- [百度C++工程师的那些极限优化（内存篇）](https://mp.weixin.qq.com/s?__biz=Mzg5MjU0NTI5OQ==&mid=2247489076&idx=1&sn=748bf716d94d5ed2739ea8a9385cd4a6&chksm=c03d2648f74aaf5e11298cf450c3453a273eb6d2161bc90e411b6d62fa0c1b96a45e411af805&scene=178&cur_album_id=1693053794688761860#rd)
- [百度C++工程师的那些极限优化（并发篇）](https://mp.weixin.qq.com/s/0Ofo8ak7-UXuuOoD0KIHwA)

## 如何贡献

如果你遇到问题或需要新功能，欢迎创建issue。

如果你可以解决某个issue, 欢迎发送PR。

发送PR前请确认有对应的单测代码。
