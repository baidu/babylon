**[[简体中文]](bounded_queue.zh-cn.md)**

# Bounded Queue

## Principle

An MPMC queue implemented on a circular array, based on the principles outlined in GlobalBalancer, featuring the following characteristics:

1. The publish operation is wait-free when the queue is not full.
2. The consume operation is wait-free when the queue is not empty.
3. For blocking publish and consume operations, subsequent actions are wait-free after recovery from blocking.

Here are some comparative evaluations of the actual performance.

## Usage Example

```c++
#include <babylon/concurrent/bounded_queue.h>

using ::babylon::ConcurrentBoundedQueue;
// Explicitly define a queue
using Queue = ConcurrentBoundedQueue<::std::string>;
Queue queue;

// Set the queue capacity; pushing will block if the capacity is exceeded
queue.reserve_and_clear(1024);

// Single element push
queue.push("10086");
// The callback version will invoke the user function to fill the data after obtaining publish rights
// However, be careful not to perform time-consuming operations in the callback, as the underlying slot will not be released until the callback function returns
queue.push([] (::std::string& target) {
    target.assign("10086");
});

// Batch push
queue.push_n(vec.begin(), vec.end());
// The callback version will invoke the user function to fill the data after obtaining publish rights
// Note that the callback function may be called multiple times; do not assume the operable data range within a single callback
queue.push_n([] (Queue::Iterator iter, Queue::Iterator end) {
    while (iter < end) {
        *iter++ = "10086";
    }
}, push_num);

// Single element pop
queue.pop(&str);
// The callback version will invoke the user function to process the data after obtaining consume rights
// However, be careful not to perform time-consuming operations in the callback, as the underlying slot will not be released until the callback function returns
queue.pop([] (::std::string& source) {
    work_on_source(source);
});

// Batch pop
queue.pop_n(vec.begin(), vec.end());
// The callback version will invoke the user function to process the data after obtaining publish rights
// Note that the callback function may be called multiple times; do not assume the operable data range within a single callback
queue.push_n([] (Queue::Iterator iter, Queue::Iterator end) {
    while (iter < end) {
        work_on_source(*iter);
    }
}, pop_num);
```
