**[[简体中文]](README.zh-cn.md)**

# Use Arena for brpc

Before invoking a user's service, [brpc](https://github.com/apache/brpc) needs to construct instances of `Request` and `Response` internally, as well as perform corresponding serialization and deserialization operations. By default, it uses dynamic heap memory allocation. For complex structures, the allocation and deallocation of memory, along with the construction and destruction of `Message` structures, can lead to noticeable overhead.

Since version 3.x, [Protobuf](https://github.com/protocolbuffers/protobuf) has introduced [Arena](https://protobuf.dev/reference/cpp/arenas) allocation, which enables aggregated allocation and deallocation for complex structures. More recent versions of [brpc](https://github.com/apache/brpc) also support the [Protobuf arena](https://github.com/apache/brpc/blob/master/docs/cn/server.md#protobuf-arena) component. Based on this, further acceleration for `string` members can be achieved by applying [arenastring](../../docs/arenastring.zh-cn.md).

In addition to using native [Arena](https://protobuf.dev/reference/cpp/arenas), you can also employ [babylon::SwissMemoryResource](../../docs/reusable/memory_resource.zh-cn.md#swissmemoryresource) for memory pool acceleration. [babylon::SwissMemoryResource](../../docs/reusable/memory_resource.zh-cn.md#swissmemoryresource) enables further flexibility through a customizable fixed-size paging reuse mechanism.

## Performance Demonstration

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
