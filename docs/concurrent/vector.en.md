**[[简体中文]](vector.zh-cn.md)**

# Vector

## Principle

Based on a segmented array, this implementation provides random access performance close to `std::vector`, as well as thread-safe size growth capabilities, ensuring that the addresses of elements obtained will not become invalid due to subsequent growth of the container. The core differences from `std::vector` include:

1. `size == capacity == n * block_size`, with size growth occurring in bulk increments defined by a pre-specified block size.
2. `&v[i] + 1 != &v[i + 1]`, allowing for random access, but the underlying space may not be contiguous.
3. Concurrent operations such as `v.ensure(i)` are thread-safe and guarantee that any address obtained through `&v.ensure(i)` will not become invalid due to subsequent growth.
4. Access and growth operations are wait-free (excluding the implementation of memory allocation and element construction).

![](images/concurrent_vector.png)

## Usage Example

```c++
#include <babylon/concurrent/vector.h>

using ::babylon::ConcurrentVector;

// Static block_size
ConcurrentVector<std::string, 128> vector;                   // Statically specify block_size as 128, must be 2^n

// Dynamic block_size
ConcurrentVector<std::string> vector;                         // ConcurrentVector<std::string, 0>(1024)
ConcurrentVector<std::string, 0> vector;                     // ConcurrentVector<std::string, 0>(1024), 0 indicates dynamic block_size, default
ConcurrentVector<std::string, 0> vector(block_size_hint);    // Actual block_size will round up block_size_hint to 2^n

// Extend capacity
vector.reserve(10010);                                        // Ensure size grows to accommodate at least 10010 elements

// Random access with potential capacity extension
vector.ensure(10086).assign("10086");                        // If current size is insufficient to hold element 10086, it will first extend the underlying storage size, then return a reference to element 10086

// Random access without capacity checking, generally used when index < size is already known
vector[10086].assign("10086");                               // If current size is insufficient to hold element 10086, it may cause an out-of-bounds error

// When multiple indices are expected to be accessed in a short period, a snapshot can be obtained to avoid repeated access to the segment mapping table
auto snapshot = vector.snapshot();
auto snapshot = vector.reserved_snapshot(30);                // Ensure elements [0, 30) are accessible
for (size_t i = 0; i < 30; ++i) {
    snapshot[i] = ... // Each access will no longer re-fetch the segment mapping table, speeding up access
}

// Copy from contiguous space similar to vector
// Optimized for the underlying segmented contiguity, similar to std::copy_n
vector.copy_n(iter, size, offset);
// Further details can be found in the comments
// Unit tests in test/test_concurrent_vector.cpp
```

## Performance Evaluation

### Sequential Write

```
==================== batch 100 ====================
std::vector loop assign use 0.212214
std::vector fill use 0.211325
babylon::ConcurrentVector loop assign use 1.26182
babylon::ConcurrentVector snapshot loop assign use 1.05421
babylon::ConcurrentVector fill use 0.219594
==================== batch 10000 ====================
std::vector loop assign use 0.288137
std::vector fill use 0.281818
babylon::ConcurrentVector loop assign use 1.18824
babylon::ConcurrentVector snapshot loop assign use 0.965977
babylon::ConcurrentVector fill use 0.304165
```

### Sequential Read

```
==================== batch 100 ====================
std::vector loop read use 0.255723
babylon::ConcurrentVector loop read use 1.36107
babylon::ConcurrentVector snapshot loop read use 1.06447
==================== batch 10000 ====================
std::vector loop read use 0.27499
babylon::ConcurrentVector loop read use 1.22806
babylon::ConcurrentVector snapshot loop read use 0.952212
```

### 12 Concurrent Reads and Writes

```
==================== seq_cst rw batch 2 ====================
std::vector use 0.342871
std::vector aligned use 0.0452792
babylon::ConcurrentVector ensure use 0.463758
babylon::ConcurrentVector [] use 0.357992
babylon::ConcurrentVector snapshot [] use 0.419337
babylon::ConcurrentVector aligned ensure use 0.045025
babylon::ConcurrentVector aligned [] use 0.047975
babylon::ConcurrentVector aligned snapshot [] use 0.0898667
==================== seq_cst rw batch 20 ====================
std::vector use 0.0754283
std::vector aligned use 0.0624383
babylon::ConcurrentVector ensure use 0.0718946
babylon::ConcurrentVector [] use 0.0718017
babylon::ConcurrentVector snapshot [] use 0.0634408
babylon::ConcurrentVector aligned ensure use 0.0610958
babylon::ConcurrentVector aligned [] use 0.0681283
babylon::ConcurrentVector aligned snapshot [] use 0.0622529
```
