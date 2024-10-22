**[[English]](allocator.en.md)**

# allocator

## MonotonicAllocator

基于memory_resource包装成为分配器，支持STL类容器的构造，类似std::pmr::polymorphic_allocator，主要区别是

- 增加了create_object接口，可以构造托管生命周期的实例类似google::protobuf::Arena的CreateMessage功能
- 兼容提供非多态接口的模式，可以节省些许虚函数开销
- babylon::SwissMemoryResource的包装特化一站式支持了构造protobuf message类型

## 使用方法

```c++
#include <babylon/reusable/allocator.h>

using ::babylon::ExclusiveMonotonicMemoryResource;
using ::babylon::MonotonicAllocator;
using ::babylon::MonotonicMemoryResource;
using ::babylon::SwissAllocator;
using ::babylon::SwissMemoryResource;

// 一个应用于T类型的分配器
// 底层使用M作为内存资源，M默认为MonotonicMemoryResource
// 构造可以传入某个子类，实际的分配是多态的
MonotonicAllocator<T> allocator(memory_resource);

// 也可以指定为实际的子类，此时分配可以绕过虚函数
MonotonicAllocator<T, ExclusiveMonotonicMemoryResource> allocator(memory_resource);

// 支持最基本allocator能力
auto* ptr = allocator.allocate(1);
allocator.construct(ptr);
...
allocator.destroy(ptr);

// 支持分配构造一体化
auto* ptr = allocator.new_object<T>(args...);
...
allocator.destroy_object(ptr);

// 支持生命周期托管
// 实际的析构会发生在memory_resource.release()时
auto* ptr = allocator.create_object<T>(args...);

// 清理memory_resource时，才会作废之前申请过的所有内存
// 托管的实例也会在此时析构
memory_resource.release()

// 支持uses allocator构造协议
struct S {
    using allocator_type = MonotonicMemoryResource<>;
    S(const std::string_view sv, allocator_type allocator) :
			allocator(allocator) {
        buffer = allocator.allocator(sv.size() + 1);
        memcpy(buffer, sv.data(), sv.size());
        buffer[sv.size()] = '\0';    
    }
    allocator_type allocator;
    char* buffer;
};
// 使用allocator方式来构建实例
auto* s = allocator.create_object<S>("12345");
s->buffer // "12345"且分配在memory_resource上

// 支持protobuf消息
SwissAllocator<> swiss_allocator(swiss_memory_resource);
auto* m = swiss_allocator.create_object<SomeMessage>();
m->GetArena(); // 构造在SwissMemoryResource内置的Arena上
```
