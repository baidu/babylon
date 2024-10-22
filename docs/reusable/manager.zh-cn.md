**[[English]](manager.en.md)**

# manager

## ReusableManager

『完美重用』对象管理器，内部包含一个MonotonicMemoryResource内存池，可以基于内存池创建实例。返回结果是对应实例的访问器而非实例指针本身，主要由于管理器会依据『完美重用』协议会周期性进行重建，这个过程中实例的指针并不固定

- 要求实例满足『完美重用』协议，即ReusableTraits::REUSABLE
- 要求实例支持MonotonicMemoryResource

实际应用中，依据每个业务生命周期，设置对应的ReusableManager实例。例如对于RPC server，可以每个请求的【上下文】中放置一个ReusableManager，请求中用到的类型由ReusableManager管理。在请求处理完成后，执行ReusableManager的清理并触发可能的重建。在持续复用ReusableManager执行请求处理的情况下，最终可以达到几乎不申请动态内存

## 用法示例

```c++
#include "reusable/manager.h"
#include "reusable/vector.h"

using ::babylon::SwissManager;
using ::babylon::SwissString;

// 创建一个管理器
SwissManager manager;

// 使用管理器创建一个实例
auto pstring = manager.create<SwissString>();

// 类似实例指针一样使用
pstring->resize(10);

// 结束使用，清理管理器
// 调用时需要确保不会并发使用任何create返回的实例
manager.clear()

// 此时已经实例恢复刚构建的状态，也可能已经重建，不要使用任何拿到的裸指针
// 返回的实例访问器可以正常继续使用
pstring->resize(100);

// 然后再清理
manager.clear();
```
