**[[简体中文]](counter.zh-cn.md)**

# Counter

## Principle

A high-concurrency counter that specializes in a write-heavy, read-light model based on `thread_local`. Each thread's operations on the counter are effective independently in its TLS. The final result is collected only during read operations, which helps to alleviate contention caused by multiple threads accessing the same memory area through TLS isolation.

## Usage Example

```c++
#include <babylon/concurrent/counter.h>

using babylon::ConcurrentAdder;
using babylon::ConcurrentMaxer;
using babylon::ConcurrentSummer;

// Construct an adder; the accumulated result is a signed 64-bit number, initialized to 0
ConcurrentAdder var;
// Multi-threaded addition and subtraction
thread1:
    var << 100;
thread2:
    var << -20;
// Get the current accumulated result
var.value(); // == 80

// Construct a maximum value recorder; the result is a signed 64-bit number
ConcurrentMaxer var;
// Get the recorded maximum value; returns 0 if no records exist during the period
ssize_t v = var.value();
// Get the recorded maximum value; returns false if no records exist during the period
var.value(v);
// Multi-threaded recording
thread1:
    var << 100;
thread2:
    var << -20;
// Get the current recorded maximum value
var.value(); // == 100
// Clear recorded results and start a new recording period
var.reset();
var << 50;
var.value(); // == 50

// Construct a summer, which provides basic accumulation functionality and can also be used to calculate averages
// The result is a signed 64-bit number, defaulting to 0
ConcurrentSummer var;
// Multi-threaded summation
thread1:
    var << 100;
thread2:
    var << -20;
// Get the current sum
var.value(); // {sum: 80, num: 2}
```
