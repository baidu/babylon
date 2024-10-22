**[[简体中文]](vector.zh-cn.md)**

# vector

A reusable `ReusableVector` that conforms to `ReusableTraits`.

When performing operations like `clear` or `pop_back`, the existing contents are not destructed. Subsequent operations like `emplace_back` or `push_back` will attempt to reuse the structure using `ReusableTraits::reconstruct`.

# Usage Example

```c++
#include "babylon/reusable/manager.h"
#include "babylon/reusable/vector.h"
#include "babylon/reusable/string.h"

using ::babylon::SwissVector;
using ::babylon::SwissString;
using ::babylon::SwissManager;

// Define a reusable manager
SwissManager manager;

// Replace std::vector<std::string>
auto pvector = manager.create<SwissVector<SwissString>>();

// Operate similarly to std::vector
pvector->emplace_back("10086");
```
