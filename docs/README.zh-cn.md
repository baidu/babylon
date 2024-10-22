**[[English]](README.en.md)**

# 模块功能文档

- [:any](any.zh-cn.md)
- [:anyflow](anyflow/README.zh-cn.md)
- [:application_context](application_context.zh-cn.md)
- [:concurrent](concurrent/README.zh-cn.md)
- [:coroutine](coroutine/README.zh-cn.md)
- [:executor](executor.zh-cn.md)
- [:future](future.zh-cn.md)
- [:logging](logging/README.zh-cn.md)
  - [Use async logger](../example/use-async-logger)
  - [Use with glog](../example/use-with-glog)
- [:reusable](reusable/README.zh-cn.md)
- [:serialization](serialization.zh-cn.md)
- [:time](time.zh-cn.md)
- Protobuf [arenastring](arenastring.zh-cn.md) patch
- Typical usage with [brpc](https://github.com/apache/brpc)
  - use [:future](future.zh-cn.md) with bthread: [example/use-with-bthread](../example/use-with-bthread)
  - use [:reusable_memory_resource](reusable/memory_resource.zh-cn.md) for rpc server: [example/use-arena-with-brpc](../example/use-arena-with-brpc)
  - use [:concurrent_counter](concurrent/counter.zh-cn.md) implement bvar: [example/use-counter-with-bvar](../example/use-counter-with-bvar)
