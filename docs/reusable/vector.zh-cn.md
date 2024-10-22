**[[English]](vector.en.md)**

# vector

可重用的ReusableVector，满足ReusableTratis

在clear/pop_back等操作时不析构已有内容，后续emplace_back/push_back等操作时使用ReusableTratis::reconstruct尽量复用结构

# 用法示例

```c++
#include "babylon/reusable/manager.h"
#include "babylon/reusable/vector.h"
#include "babylon/reusable/string.h"

using ::babylon::SwissVector;
using ::babylon::SwissString;
using ::babylon::SwissManager;

// 定义一个重用管理器
SwissManager manager;

// 替换std::vector<std::string>
auto pvector = manger.create<SwissVector<SwissString>>();

// 等用于std::vector进行操作
pvector->emplace_back("10086");
```
