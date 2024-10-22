**[[简体中文]](execution_queue.zh-cn.md)**

# Execution Queue

## Principle

This wraps the [ConcurrentBoundedQueue](bounded_queue.en.md) to implement an on-demand activation model for MPSC (Multi-Producer Single-Consumer) consumers:
- Each time data is produced, an atomic increment is performed on the pending counter. If the value before incrementing is 0, a rising edge is triggered to start the consumer thread.
- The consumer thread continuously consumes data; when there is no data to consume, it exchanges the counter value with 0.
- If the exchanged value remains unchanged from the value recorded before the last consumption, the consumer thread exits.
- Otherwise, it enters the next round of the consumption loop.

This is mainly used to support scenarios with a large number of low-activity queues, saving inactive listener consumer threads. Its functionality is similar to [bthread::ExecutionQueue](https://github.com/apache/brpc/blob/master/docs/en/execution_queue.md), but:
- Consumers submit through the [Executor](../executor.en.md) interface, supporting the use of custom thread/coroutine mechanisms.
- It maintains wait-free submission for producers without causing consumer blocking during contention.

![](images/bthread_execution_queue.png)

## Usage Example

```c++
#include "babylon/concurrent/execution_queue.h"

using ::babylon::ConcurrentExecutionQueue;

// Explicitly define a queue
using Queue = ConcurrentExecutionQueue<T>;
Queue queue;

// Set the queue capacity to N
// The consumer uses some_executor for execution
// Register a lambda function for consumption
queue.initialize(N, some_executor, [] (Queue::Iterator iter, Queue::Iterator end) {
  // Consume the data in the range
  while (iter != end) {
    T& item = *iter;
    do_sth_with(item);
    ++iter;
  }
});

// Produce a piece of data and start background consumption as needed
queue.execute("10086");
...

// Wait for all currently published data to be consumed
// Note that this does not include stop semantics; you can repeatedly execute & join
queue.join();
```
