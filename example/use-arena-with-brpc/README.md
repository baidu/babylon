# Use arena for brpc

brpc在调用用户的service前，需要在内部先完成Request和Response的实例构建和以及前后对应的正反序列化。对应的代码实现在相应的Protocol中，默认的方式实例采用动态堆内存分配模式创建，对于比较复杂的结构，内存分配释放和Message结构的构建和析构可能也会带来可见的开销。

利用babylon::SwissMemoryResource可以将堆内存分配该用Arena机制分配到内存池上，降低内存分配的成本且提升局部性。进一步使用babylon::ReusableManager可以在保持内存池聚集分配的局部性的同时，进一步通过尽可能复用Message结构降低构建和析构的开销。

下面实现了一个集成了对应功能的brpc::Protocol，演示了响应功能的使用方式，并配套对应的例子来演示性能对比。相应的Protocol实际也在baidu内部广泛使用，预期可以支持在生产环境直接使用。

## 示例构成

- `:reusable_rpc_protocol`: 独立的brpc::Protocol实现，集成了内存复用和实例复用的功能
  - `reusable_rpc_protocol.h`&`reusable_rpc_protocol.cpp`: 独立逻辑，和对应brpc版本无关
  - `reusable_rpc_protocol.trick.cpp`: 拷贝自`src/brpc/policy/baidu_rpc_protocol.cpp`并进行简要修改
- `:client`&`:server`: 模拟比较复杂的Message演示性能对比

## 使用手册

```
#include "reusable_rpc_protocol.h"

// 向brpc注册新的protocol，默认使用
// protocol type = 72
// protocol name = "baidu_std_reuse"
if (0 != ::babylon::ReusableRPCProtocol::register_protocol()) {
	// 注册protocol失败
}
// 返回失败很可能因为type冲突，可以更换type和name
if (0 != ::babylon::ReusableRPCProtocol::register_protocol(type, name)) {
	// 注册protocol失败
}

// ReusableRPCProtocol协议和baidu_std相同，注册后，默认依然会走baidu_std
// 需要通过显式在option中指定来启用
::baidu::rpc::ServerOptions options;
options.enabled_protocols = "baidu_std_reuse";

// 下面正常注册服务，启动服务器即可
class SomeServiceImpl : public SomeService {
 public:
	virtual void some_method(::google::protobuf::RpcController* controller,
			const SomeRequest* request,
			SomeResponse* response,
			::google::protobuf::Closure* done) {
		... // 正常进行业务处理，对应的request和response已经改用内存池或者实例复用托管了
	}
};

// 影响运行时的flag
// --babylon_rpc_full_reuse，是否启用实例重用，默认false
// --babylon_rpc_closure_cache_num，内存池和ReusableManager实例本身也会通过对象池复用，设置对象池大小
// --babylon_rpc_page_size，内存池单页大小，超过单页大小的申请会直接改为动态申请
// --babylon_rpc_page_cache_num，内存池页本身通过对象池复用，设置对象池大小
```

## 性能演示

CPU: AMD EPYC 7W83 64-Core Processor, taskset 0-3 core

QPS: 750

- 原始模式
  - latency_percentiles: "[1923,2222,2944,3447]"
  - process_cpu_usage : 1.489
  - process_memory_resident : 59244544
- `--use_arena`模式
  - latency_percentiles: "[1378,1607,2263,2716]"
  - process_cpu_usage : 0.695
  - process_memory_resident : 54255616
- `--use_arena`&`--babylon_rpc_full_reuse`模式
  - latency_percentiles: "[1096,1256,1612,1938]"
  - process_cpu_usage : 0.612
  - process_memory_resident : 101576704
