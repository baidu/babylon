**[[简体中文]](object_pool.en.md)**

# Object Pool

## Principle

The object pool is a typical implementation designed to support application scenarios where:

1. A parallel set of computations requires a specific type of resource.
2. Instances of this resource need to be exclusively used while in operation.
3. Creating and maintaining such resources is relatively "expensive."

Typical examples include sockets used for communication and storage structures used in model inference.

The underlying implementation is based on a bounded queue to provide wait-free resource allocation and return. Two modes are wrapped based on typical scenarios:

1. **Strict Limited Mode**: A fixed number of resource instances (N) are pre-injected. If the number of applicants exceeds N, subsequent applicants must wait for others to return resources before obtaining one.
2. **Automatic Creation Mode**: When there are insufficient resources in the object pool, applicants will receive newly created temporary resource instances. When the free amount in the object pool exceeds N, returned resource instances will be released.

## Usage Example

```c++
#include "babylon/concurrent/object_pool.h"

// The default constructed object pool has a capacity of 0 and must be configured before use
::babylon::ObjectPool<R> pool;

// Set the maximum capacity
pool.reserve_and_clear(N);

// Set the recycling function; once set, the recycling function will be automatically called for instances entering the object pool
// Note: In automatic creation mode, instances that are directly destroyed after overflow will also be cleaned up before destruction
pool.set_recycler([] (R& resource) {
    // Clean up the resource instance
});

/////////////////
// Strict Limited Mode
for (...) {
    // Pre-inject a fixed number of instances
    pool.push(std::make_unique<R>(...));
}
parallel loop:
    // Use pop to get an instance; if the object pool is empty, it will block until an instance is returned
    auto ptr = pool.pop();
    ptr->...; // The return value is a smart pointer with a custom deleter
    // Instances are automatically returned upon destruction
/////////////////

/////////////////
// Automatic Creation Mode
// Enable automatic creation mode by setting a constructor callback
pool.set_creator([] {
    return new R(...);
});
parallel loop:
    // Use pop to get an instance; if the object pool is empty, the constructor callback will be automatically called
    auto ptr = pool.pop();
    ptr->...; // The return value is a smart pointer with a custom deleter
    // Instances are automatically returned upon destruction; if the number of instances in the pool exceeds capacity N, the excess will be destroyed directly
/////////////////
```
