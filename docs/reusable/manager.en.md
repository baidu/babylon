**[[简体中文]](manager.zh-cn.md)**

# manager

## ReusableManager

A "perfect reuse" object manager that internally contains a `MonotonicMemoryResource` memory pool, allowing instances to be created based on the pool. The result returned is an accessor to the corresponding instance, not the instance pointer itself. This is because the manager periodically reconstructs instances according to the "perfect reuse" protocol, meaning the instance pointers are not fixed during this process.

- The instances are required to comply with the "perfect reuse" protocol, i.e., `ReusableTraits::REUSABLE`.
- The instances must support `MonotonicMemoryResource`.

In practical applications, a `ReusableManager` instance is set up according to each business lifecycle. For example, in an RPC server, a `ReusableManager` can be placed in the context of each request, and the types used during the request are managed by the `ReusableManager`. After the request is processed, `ReusableManager` is cleared, potentially triggering reconstruction. In the long run, as `ReusableManager` is continuously reused for processing requests, it can achieve nearly zero dynamic memory allocation.

## Example Usage

```c++
#include "reusable/manager.h"
#include "reusable/vector.h"

using ::babylon::SwissManager;
using ::babylon::SwissString;

// Create a manager
SwissManager manager;

// Use the manager to create an instance
auto pstring = manager.create<SwissString>();

// Use it like a regular instance pointer
pstring->resize(10);

// Finish using, and clear the manager
// Ensure that no instances returned by create are being concurrently used when calling clear
manager.clear()

// At this point, the instance has reverted to its initial state and may have been reconstructed
// Do not use any raw pointers obtained before
// The returned instance accessor can continue to be used as normal
pstring->resize(100);

// Then clear again
manager.clear();
```
