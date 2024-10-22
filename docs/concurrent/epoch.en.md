**[[简体中文]](epoch.zh-cn.md)**

# Epoch

## Principle

In typical lock-free structure implementations, there are generally two aspects that need to be addressed:
- Reducing the critical section of each concurrent access action to converge into a single CAS or FAA action, ultimately utilizing hardware atomic instructions for lock-free cooperative access. How this is achieved is often directly related to the target data structure and application scenario; most lock-free algorithms focus on this aspect of implementation.
- Since there are no programmable multi-instruction critical sections, in the implementation of "removal" actions, while it can be ensured that concurrent accesses can correctly and completely retrieve the elements "before" or "after" the action, it is impossible to track whether the removed elements are "still held," thus unable to confirm "when they can be released." Most lock-free algorithm descriptions assume that an external environment has implemented automatic identification and release of useless elements, essentially automatic garbage collection.

Lock-free algorithm garbage collection is less related to data structures and application scenarios and is generally considered as a separate topic. For example, the article [Performance of memory reclamation for lockless synchronization](https://sysweb.cs.toronto.edu/publication_files/0000/0159/jpdc07.pdf) provides a review of some typical lock-free reclamation methods. It mentions several classic reclamation schemes: QSBR (Quiescent-state-based reclamation), EBR (Epoch-based reclamation), and HPBR (Hazard-pointer-based reclamation).

Epoch is a modified implementation of the EBR mechanism, which focuses on reducing the overhead caused by mechanisms through the separation of access, elimination, and reclamation actions. The main differences are:
- Changing from the classic three-epoch loop to an infinitely increasing one, supporting asynchronous reclamation.
- Accesses not involving elimination no longer advance the epoch, reducing the overhead of a memory barrier.
- Access, elimination, and reclamation actions all support aggregated execution, further reducing the overhead of memory barriers.

![](images/epoch.png)

## Usage Example

```c++
#include "babylon/concurrent/epoch.h"

using ::babylon::Epoch;

// Define an Epoch instance
Epoch epoch;

// Use Epoch to open a thread-local critical section before accessing the shared structure
{
  std::lock_guard<Epoch> lock {epoch};
  ... // Elements of the shared structure can be accessed within the critical section
} // After the critical section ends, the obtained element pointer can no longer be used

// In addition to thread-local mode, an Accessor can also be actively created to track the critical section
auto accessor = epoch.create_accessor();
{
  std::lock_guard<Accessor> lock {accessor};
  ... // Elements of the shared structure can be accessed within the critical section
} // After the critical section ends, the obtained element pointer can no longer be used

{
  accessor.lock();
  Task task {::std::move(accessor), ...} // The critical section can be transferred by moving the accessor
  ... // It can be transferred to an asynchronous thread, etc. The critical section ends after accessor.unlock()
} // If transferred, the critical section will not end here

// Elimination operation
... // Operate on the shared structure, removing some elements
// Advance the epoch and return the minimum epoch at which the previously removed elements can be reclaimed
auto lowest_epoch = epoch.tick();

// The lowest_epoch is associated with the eliminated elements, which can be synchronously or asynchronously checked for actual reclamation
auto low_water_mark = epoch.low_water_mark();
// When low_water_mark exceeds lowest_epoch, the elements can be safely reclaimed
if (lowest_epoch <= low_water_mark) {
  ... // Elements corresponding to lowest_epoch can be reclaimed
}
```
