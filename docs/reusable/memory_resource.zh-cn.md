**[[English]](memory_resource.en.md)**

# memory_resource

## 原理

单调内存资源，功能设计上等同于std::pmr::monotonic_buffer_resource，都继承自std::pmr::memory_resource以便支持std::pmr::polymorphic_allocator机制，采用统一容器类型，支持对接不同分配器实现，而不同点主要是

- 类似google::protobuf::Arena提供注册析构函数的功能，除了内存分配，也可以托管实例的生命周期
- 提供线程安全和线程不安全两种实现，线程安全版本通过thread_local缓存来减轻并发竞争
- 底层统一对接到定长页分配器page_allocator接口，而非std::pmr::monotonic_buffer_resource和google::protobuf::Arena采用的变长分配，有助于进一步去除malloc压力

### ExclusiveMonotonicBufferResource

![](images/exclusive.png)

独占的单调内存资源，是单调内存资源的基础实现，线程不安全，实现上将多次申请动作打包成整页申请向底层发起，加速大量零碎分配的场景

### SharedMonotonicBufferResource

![](images/shared.png)

由一系列线程局部的独占内存资源构成，每个线程在分配时使用对应的独占资源完成

### SwissMemoryResource

SharedMonotonicBufferResource的一个轻量级继承，不过同时能够支持当做google::protobuf::Arena来使用；这个支持通过patch了protobuf的内部实现来完成，patch会尽可能自动生效，如果版本不支持，或者链接顺序等问题导致patch不生效的场景，会退化成使用一个真实的google::protobuf::Arena；依然保证功能正确可用，只是无法统一通过PageAllocator分配

## 使用方法

```c++
#include <babylon/reusable/memory_resource.h>

using ::babylon::PageAllocator;
using ::babylon::ExclusiveMonotonicBufferResource;
using ::babylon::SharedMonotonicBufferResource;
using ::babylon::SwissMemoryResource;

// 默认构造时采用new/delete直接从系统整页申请内存
ExclusiveMonotonicBufferResource resource;
SharedMonotonicBufferResource resource;
SwissMemoryResource resource;

// 指定零碎内存聚合后使用的整页分配器，替换默认的SystemPageAllocator
PageAllocator& page_allocator = get_some_allocator();
resource.set_page_allocator(page_allocator);

// 指定上游分配器，大块内存直接从上游申请，替换默认的std::pmr::new_delete_resource();
std::pmr::memory_resource& memory_resrouce = get_some_resource();
resource.set_upstream(memory_resrouce);

// 可以作为一种memory_resource直接支持std::pmr容器
::std::pmr::vector<::std::pmr::string> pmr_vector(&resource);

// 也可以直接用来申请内存
resource.allocate(bytes, alignment);

// 可以通过模板传入alignment进一步加速对齐计算
resource.allocate<alignment>(bytes);

// 可以注册析构函数到内存资源，在下一次release调用时，会调用
// instance.~T()完成析构，注意，这里仅仅会【调用析构函数】，而不会尝试释放instance本身的内存
// 一般仅用于封装出类似google::protobuf::Arena的Create语义的接口
T* instance;
resource.register_destructor(instance);

// 注册析构函数的type erased版本，支持注册任意自定义的析构方法
void destruct(void* instance) {
    reinterpret_cast<T*>(instance)->~T();
}
resource.register_destructor(instance, destruct);

// release时首先调用所有注册的析构函数，之后将分配过的内存整体释放掉
// 如果有使用这个内存资源的std::pmr容器，需要保证在release前都已析构
resource.release();

// SwissMemoryResource独有功能，可以隐式转换为arena使用
google::protobuf::Arena& arena = swiss_memory_resource;
T* message_on_arena = google::protobuf::Arena::CreateMessage<T>(&arena);
```
