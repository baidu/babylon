**[[English]](README.en.md)**

# Use arena for brpc

[brpc](https://github.com/apache/brpc)在调用用户的service前，需要在内部先完成Request和Response的实例构建，并在service前后执行对应的正反序列化。默认采用动态堆内存分配模式创建，对于比较复杂的结构，内存分配释放和Message结构的构建和析构可能也会带来可见的开销。

[Protobuf](https://github.com/protocolbuffers/protobuf)在3.x之后增加了[Arena](https://protobuf.dev/reference/cpp/arenas)分配功能，针对复杂结构提供了聚集分配和释放能力。较新版本的[brpc](https://github.com/apache/brpc)也提供了[Protobuf arena](https://github.com/apache/brpc/blob/master/docs/cn/server.md#protobuf-arena)组件进行支持。在这些基础上，通过应用[arenastirng](../../docs/arenastring.zh-cn.md)可以针对string成员实现进一步加速。

除了使用原生的[Arena](https://protobuf.dev/reference/cpp/arenas)，也可以使用[babylon::SwissMemoryResource](../../docs/reusable/memory_resource.zh-cn.md#swissmemoryresource)实现内存池加速。[babylon::SwissMemoryResource](../../docs/reusable/memory_resource.zh-cn.md#swissmemoryresource)通过可定制的定长分页重用机制，可以进一步提升灵活性。

## 性能演示

CPU: AMD EPYC 7W83 64-Core Processor, taskset 0-3 core

QPS: 800

- Default (mode: 0)
  - latency_percentiles: "[2213,2523,3232,3670]"
  - process_cpu_usage : 1.172
  - process_memory_resident : 44978722
- Arena (mode: 1)
  - latency_percentiles: "[1318,1490,1794,1984]"
  - process_cpu_usage : 0.702
  - process_memory_resident : 41421824
- Arena & ArenaString (mode: 1, arenastring patch)
  - latency_percentiles: "[1055,1196,1416,1583]"
  - process_cpu_usage : 0.572
  - process_memory_resident : 39732770
- SwissMemoryResource & ArenaString (mode: 2, arenastring patch)
  - latency_percentiles: "[1006,1139,1341,1478]"
  - process_cpu_usage : 0.551
  - process_memory_resident : 44763136
