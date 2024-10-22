**[[简体中文]](transient_topic.zh-cn.md)**

# Transient Topic

## Principle

The MPSC (Multiple Producer, Single Consumer) Pub-Sub Topic implemented on continuous space does not support multi-threaded contention on the consumer side but allows multiple consumers to independently and repeatedly subscribe once. The core difference from a typical Pub-Sub Topic is its commitment to support "transience," meaning it is expected to be used periodically, with a limited amount of published data in each usage cycle, clearing all data before the next cycle arrives. For example, it can support localized "transient" Pub-Sub parallel computation within a single RPC.

In implementation, a vector is used to store the published data, and a mechanism similar to a bounded queue with segmented positions is used to coordinate publishing and consumption.

![](images/transient_topic.png)

## Usage Example

```c++
#include <babylon/concurrent/transient_topic.h>

using ::babylon::ConcurrentTransientTopic;

// Explicitly define a Topic
ConcurrentTransientTopic<::std::string> topic;

// Reserve space of length N
// When repeatedly using the same topic instance, it retains the previous space
// Therefore, it is generally unnecessary to specifically reserve
topic.reserve(N);

// Publish data, thread-safe
threads:
    topic.publish(V);  // Single publish
    // Batch publish
    topic.publish_n(N, [] (Iter begin, Iter end) {
        ... // Fill the results to be published in [begin, end), which will be officially published upon return
            // This may be called multiple times, each time passing a sub-range, with the total reaching N
    });

// End publishing; consumers will be notified of the end and will exit the consumption loop
topic.close();

// Create a consumer
// Multiple creations can yield multiple independent consumers, each can consume the full data set once
auto consumer = topic.subscribe();

// Consume 1 element; returns a pointer to the element, blocking and waiting if there is no publication
// Returns nullptr after consumption
auto item = consumer.consume();

// Batch consume num elements; returns a consumable range, blocking and waiting if there is insufficient publication
// Unless the queue is closed early, the returned range may be less than num
auto range = consumer.consume(num);
for (size_t i = 0; i < range.size(); ++i) {
    auto& item = range[i]; // Get a reference to the i-th element in this batch
}

// Clear the queue for reuse in the next publication
topic.clear();
```
