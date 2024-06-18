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
- COMPILER: gcc-9/gcc-10/gcc-12/clang-10/clang-14

### Bazel

Babylon使用[Bazel](https://bazel.build)进行构建并使用[bzlmod](https://bazel.build/external/module)进行依赖管理，考虑到目前Bazel生态整体处于bzlmod的转换周期，Babylon也依然兼容[workspace](https://bazel.build/rules/lib/globals/workspace)依赖管理模式

#### bzlmod

- 增加仓库注册表
```
# in .bazelrc
# babylon自身发布在独立注册表
common --registry=https://baidu.github.io/babylon/registry
# babylon依赖的boost发布在bazelboost项目的注册表，当然如果有其他源可以提供boost的注册表也可以替换
# 只要同样能够提供boost.preprocessor和boost.spirit模块寻址即可
common --registry=https://raw.githubusercontent.com/bazelboost/registry/main
```
- 增加依赖项
```
# in MODULE.bazel
bazel_dep(name = 'babylon', version = '1.1.6')
```
- 添加依赖的子模块到编译目标，全部可用子模块可以参照[模块功能文档](#模块功能文档)，或者[BUILD](BUILD)文件
```
# in BUILD
cc_library(
  ...
  deps = [
    ...
    '@babylon//:any',
    '@babylon//:concurrent',
  ],
)
```
- 也可以直接添加All in One依赖目标`@babylon//:babylon`
- 在[example/depend-use-bzlmod](example/depend-use-bzlmod)可以找到一个如何通过bzlmod依赖的简单样例

#### workspace

- 增加babylon依赖项
```
# in WORKSPACE
http_archive(
  name = 'com_baidu_babylon',
  urls = ['https://github.com/baidu/babylon/archive/refs/tags/v1.1.6.tar.gz'],
  strip_prefix = 'babylon-1.1.6',
  sha256 = 'a5bbc29f55819c90e00b40f9b5d2716d5f0232a158d69c530d8c7bac5bd794b6',
)
```
- 增加传递依赖项，内容拷贝自babylon代码库的WORKSPACE，并和项目自身依赖项合并
```
# in WORKSPACE
... // 增加babylon的WORKSPACE内的依赖项，注意和项目已有依赖去重合并
```
- 添加依赖的子模块到编译目标，全部可用子模块可以参照[模块功能文档](#模块功能文档)，或者[BUILD](BUILD)文件
```
# in BUILD
cc_library(
  ...
  deps = [
    ...
    '@com_baidu_babylon//:any',
    '@com_baidu_babylon//:concurrent',
  ],
)
```
- 也可以直接添加All in One依赖目标`@com_baidu_babylon//:babylon`
- 在[example/depend-use-workspace](example/depend-use-workspace)可以找到一个如何通过workspace依赖的简单样例

### CMake

Babylon也支持使用[CMake](https://cmake.org)进行构建，并支持通过[find_package](https://cmake.org/cmake/help/latest/command/find_package.html)、[add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)或[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)进行依赖引入

#### FetchContent

- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BUILD_DEPS ON)

include(FetchContent)
FetchContent_Declare(
  babylon
  URL "https://github.com/baidu/babylon/archive/refs/tags/v1.1.6.tar.gz"
  URL_HASH SHA256=a5bbc29f55819c90e00b40f9b5d2716d5f0232a158d69c530d8c7bac5bd794b6
)
FetchContent_MakeAvailable(babylon)
```
- 添加依赖到编译目标，CMake编译目前只提供All in One依赖目标`babylon::babylon`
```
# in CMakeList.txt
target_link_libraries(your_target babylon::babylon)
```
- 编译目标项目
  - `cmake -Bbuild`
  - `cmake --build build -j$(nproc)`
- 在[example/depend-use-cmake-fetch](example/depend-use-cmake-fetch)可以找到一个如何通过CMake FetchContent依赖的简单样例

#### find_package

- 编译并安装`boost` `abseil-cpp` `protobuf`或者直接使用apt等包管理工具安装对应平台的预编译包
- 编译并安装babylon
  - `cmake -Bbuild -DCMAKE_INSTALL_PREFIX=/your/install/path -DCMAKE_PREFIX_PATH=/your/install/path`
  - `cmake --build build -j$(nproc)`
  - `cmake --install build`
- 增加依赖项到目标项目
```
# in CMakeList.txt
find_package(babylon REQUIRED)
```
- 添加依赖到编译目标，CMake编译目前只提供All in One依赖目标`babylon::babylon`
```
# in CMakeList.txt
target_link_libraries(your_target babylon::babylon)
```
- 编译目标项目
  - `cmake -Bbuild -DCMAKE_PREFIX_PATH=/your/install/path`
  - `cmake --build build -j$(nproc)`
- 在[example/depend-use-cmake-find](example/depend-use-cmake-find)可以找到一个如何通过CMake find_package依赖的简单样例

#### add_subdirectory

- 下载`boost` `abseil-cpp` `protobuf``babylon`源码
- 增加依赖项到目标项目
```
# in CMakeList.txt
set(BOOST_INCLUDE_LIBRARIES preprocessor spirit)
add_subdirectory(boost EXCLUDE_FROM_ALL)
set(ABSL_ENABLE_INSTALL ON)
add_subdirectory(abseil-cpp)
set(protobuf_BUILD_TESTS OFF)
add_subdirectory(protobuf)
add_subdirectory(babylon)
```
- 添加依赖到编译目标，CMake编译目前只提供All in One依赖目标`babylon::babylon`
```
# in CMakeList.txt
target_link_libraries(your_target babylon::babylon)
```
- 编译目标项目
  - `cmake -Bbuild`
  - `cmake --build build -j$(nproc)`
- 在[example/depend-use-cmake-subdir](example/depend-use-cmake-subdir)可以找到一个如何通过CMake sub_directory依赖的简单样例

## 模块功能文档

- [:any](docs/any.md)
- [:anyflow](docs/anyflow/index.md)
- [:application_context](docs/application_context.md)
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
