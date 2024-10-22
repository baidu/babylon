**[[简体中文]](id_allocator.zh-cn.md)**

# Id Allocator

## Principle

The Id Allocator is used to assign a unique numerical identifier to each instance of a resource, with values concentrated in the range [0, total number of instances). This concept is similar to Unix's File Descriptor. To address the ABA problem that arises from resource reuse during creation and destruction, a versioning mechanism is added. It is primarily used to accelerate certain resource management operations.

For example, in scenarios where additional information needs to be recorded for resources (such as attaching connection status information to sockets), the continuity of numerical identifiers allows for addressing using arrays rather than hash tables, improving time and space efficiency.

The implementation uses a lock-free stack to organize free identifiers, combined with an atomic variable.

![](images/id_allocator.png)

## Usage Example

### IdAllocator

```c++
#include <babylon/concurrent/id_allocator.h>

using ::babylon::IdAllocator;

// Define an allocator that supports both 32-bit and 16-bit versions; the 16-bit version is primarily used for thread ID implementations (it's unlikely that a reasonably designed program would use more than 65536 threads simultaneously)
IdAllocator<uint32_t> allocator;
IdAllocator<uint16_t> allocator;

// Allocate an id, ensuring the id value is unique within the currently allocated but unfreed set; ensure version + id is unique within the visible competition range of the program
auto versioned_value = allocator.allocate();
// versioned_value.value = allocated id value
// versioned_value.version = allocated version

// Deallocate an id
allocator.deallocate(versioned_value);
// You can also pass just the id value, as it can be guaranteed to be unique within the allocated set; the actual deallocation interface does not use the version part
allocator.deallocate(VersionedValue<T>{value});

// Get the upper limit of allocated ids; the range [0, end_id) indicates the range of previously allocated id values
// This can be used for traversing operations
auto end_value = allocator.end();

// Traverse the currently allocated but unfreed ids
allocator.for_each([] (uint32_t begin, uint32_t end) {
    // [begin, end) indicates a range of active ids
    // The callback will be called multiple times for each non-empty range during a single for_each call
});
```

### ThreadId

Utilize `babylon::IdAllocator` to assign a unique id for each thread, taking advantage of the characteristics of `babylon::IdAllocator` to ensure that the ids are as small and continuous as possible.

```c++
#include <babylon/concurrent/id_allocator.h>

using ::babylon::ThreadId;

// Obtain the current thread's id; when a thread exits, its used id will be reused by subsequent new threads
VersionedValue<uint16_t> thread_id = ThreadId::current_thread_id();

// Get the upper limit of previously active thread ids; the range [0, end_id) indicates the range of ids that have been active
// "Previously active" is defined as having called ThreadId::current_thread_id
// However, these threads may currently still be running or may have already exited
uint16_t end_id = ThreadId::end();

// Traverse the currently active thread ids, defined as those that have called ThreadId::current_thread_id and have not exited
ThreadId::for_each([] (uint16_t begin, uint16_t end) {
    // [begin, end) indicates a range of active thread ids
    // The callback will be called multiple times for each non-empty range during a single for_each call
});
```
