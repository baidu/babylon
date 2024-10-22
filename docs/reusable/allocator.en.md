**[[简体中文]](allocator.zh-cn.md)**

# allocator

## MonotonicAllocator

A memory allocator based on `memory_resource`, supporting the construction of STL containers, similar to `std::pmr::polymorphic_allocator`. The main differences are:

- Added `create_object` interface, which allows constructing instances with managed lifetimes, similar to `google::protobuf::Arena`'s `CreateMessage` functionality.
- Compatible with a non-polymorphic interface mode, which can save some virtual function overhead.
- Specially wraps `babylon::SwissMemoryResource` to provide one-stop support for constructing protobuf message types.

## Usage

```c++
#include <babylon/reusable/allocator.h>

using ::babylon::ExclusiveMonotonicMemoryResource;
using ::babylon::MonotonicAllocator;
using ::babylon::MonotonicMemoryResource;
using ::babylon::SwissAllocator;
using ::babylon::SwissMemoryResource;

// An allocator for type T
// Uses M as the underlying memory resource, with M defaulting to MonotonicMemoryResource
// The constructor can take a subclass, and the actual allocation is polymorphic
MonotonicAllocator<T> allocator(memory_resource);

// You can also specify an actual subclass, allowing allocation to bypass virtual functions
MonotonicAllocator<T, ExclusiveMonotonicMemoryResource> allocator(memory_resource);

// Supports basic allocator functionality
auto* ptr = allocator.allocate(1);
allocator.construct(ptr);
...
allocator.destroy(ptr);

// Supports allocation and construction in one step
auto* ptr = allocator.new_object<T>(args...);
...
allocator.destroy_object(ptr);

// Supports lifetime management
// Actual destruction occurs when memory_resource.release() is called
auto* ptr = allocator.create_object<T>(args...);

// Memory allocated remains valid until memory_resource is released
// Managed instances are also destructed at this point
memory_resource.release();

// Supports "uses allocator" construction protocol
struct S {
    using allocator_type = MonotonicMemoryResource<>;
    S(const std::string_view sv, allocator_type allocator) :
			allocator(allocator) {
        buffer = allocator.allocate(sv.size() + 1);
        memcpy(buffer, sv.data(), sv.size());
        buffer[sv.size()] = '\0';
    }
    allocator_type allocator;
    char* buffer;
};
// Construct an instance using the allocator
auto* s = allocator.create_object<S>("12345");
s->buffer // "12345" and allocated on memory_resource

// Supports protobuf messages
SwissAllocator<> swiss_allocator(swiss_memory_resource);
auto* m = swiss_allocator.create_object<SomeMessage>();
m->GetArena(); // Constructed in the Arena built into SwissMemoryResource
```
