**[[简体中文]](garbage_collector.zh-cn.md)**

# Garbage Collector

## Principle

Based on the [Epoch](epoch.en.md) mechanism, a classic synchronized reclamation mechanism can be implemented. However, the modifications in Epoch focus more on achieving asynchronous reclamation. The GarbageCollector is the implementation of this asynchronous reclamation scheme. It primarily designs the retire operation interface, which packages reclamation actions into tasks and binds them to the corresponding epoch before placing them into a [ConcurrentBoundedQueue](bounded_queue.en.md) for asynchronous reclamation. An independent asynchronous reclamation thread continuously monitors the current epoch's low water mark, retrieves tasks from the queue, validates them, and ultimately executes the reclamation tasks.

## Usage Example

```c++
#include "babylon/concurrent/garbage_collector.h"

using ::babylon::Epoch;
using ::babylon::GarbageCollector;

// Define an instance of GarbageCollector, with the template parameter being the type of the reclamation action instance
// Requires std::invocable(Reclaimer)
GarbageCollector<Reclaimer> garbage_collector;

// Adjust the asynchronous queue length; the default length is 1, but it usually needs to be set longer based on actual conditions
// When the asynchronous queue is full, the retire actions will start blocking until the asynchronous reclamation completes
garbage_collector.set_queue_capacity(...);

// Start the asynchronous reclamation thread
garbage_collector.start();

// Execute the retire action, which internally performs an Epoch::tick and binds the reclaimer to the queue
// For bulk reclamation, you can perform an Epoch::tick externally and pass the returned value as lowest_epoch
// Retire actions are not required to be executed within the epoch critical section; they are even encouraged to be executed outside the critical section to avoid deadlock caused by a too-short queue during blocking
garbage_collector.retire(::std::move(reclaimer));
garbage_collector.retire(::std::move(reclaimer), lowest_epoch);

// End the asynchronous reclamation thread, waiting for all queued retire tasks to complete reclamation
garbage_collector.stop();
```
