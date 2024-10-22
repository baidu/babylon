**[[简体中文]](deposit_box.zh-cn.md)**

# Deposit Box

## Principle

Sometimes we need a design pattern where multiple callers dynamically compete to complete the same piece of logic. Typical implementations include timeout actions (one valid, one invalid) or backup request actions (two valid, first come first served). In principle, the mechanism required for this pattern is very similar to [std::call_once](https://en.cppreference.com/w/cpp/thread/call_once), but there are a few key differences:
- The latecomers do not need to wait for the executor to finish; they can simply abandon their execution.
- Since latecomers do not require any execution actions or results, the executor can release resources for reuse earlier. Accordingly, latecomers must ensure that they do not modify resources that may have already been reused.

![](images/deposit_box.png)

This is implemented by organizing actual data with [IdAllocator](id_allocator.en.md) and [ConcurrentVector](vector.en.md), enabling aggregate storage and fast access based on index. In each round, the take action competes for ownership through a CAS increment of the version number, with latecomers touching only the CAS action and not the data part. The version number itself is also stored in the slot of [ConcurrentVector](vector.en.md), ensuring that the latecomer’s CAS action is legitimate, while the monotonic increase characteristic of the version number eliminates the ABA problem for latecomers.

## Usage Example

### DepositBox

```c++
#include <babylon/concurrent/deposit_box.h>

using ::babylon::DepositBox;

// Only supports usage through a global singleton
auto& box = DepositBox<Item>::instance();

// Allocate a slot and construct an element in it using Item(...), returning an id for future competitive retrieval of this element
// Concurrent multiple emplace actions are thread-safe
auto id = box.emplace(...);

{
  // The retrieval operation can compete to execute on the same id, and is thread-safe
  auto accessor = box.take(id);
  // Whether the accessor is non-null can determine if it is the first visitor to acquire ownership
  // A non-null accessor can further be used to access the element
  if (accessor) {
    accessor->item_member_function(...);
    some_function_use_item(*accessor);
  }
} // The accessor destructs, releasing the slot; the element pointer is no longer available

///////////////////////////// Advanced Usage /////////////////////////////////////

// Non-RAII mode, directly returns the element pointer Item*
auto item = box.take_released(id);
if (item) {
  item->item_member_function(...);
  some_function_use_item(*item);
}
// Ownership must be explicitly released when no longer needed
box.finish_released(id);

// This does not check or operate on the version number part and directly obtains the element pointer corresponding to the id slot, thus cannot safely be concurrent with take actions
// In some scenarios where elements cannot be fully prepared via the constructor, further incremental operations on the element can be performed after allocating the slot
// However, the user must ensure that the id has not yet been given to a competing visitor for a take action
auto item = box.unsafe_get(id);
item->item_member_function(...);
some_function_use_item(*item);
```
