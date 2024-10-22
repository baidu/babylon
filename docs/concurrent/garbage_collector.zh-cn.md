**[[English]](garbage_collector.en.md)**

# garbage_collector

## 原理

基于[Epoch](epoch.zh-cn.md)机制，可以实现经典的同步回收机制，不过Epoch的修改更着重于实现异步回收，GarbageCollector是这个异步回收方案的实现；主要设计了retire操作接口，将回收动作打包成任务并绑定对应的epoch之后放入[ConcurrentBoundedQueue](bounded_queue.zh-cn.md)等待异步回收；独立的异步回收线程持续进行当前epoch最低水位的检测，并从队列获取、校验并最终执行回收任务；

## 用法示例

```c++
#include "babylon/concurrent/garbage_collector.h"

using ::babylon::Epoch;
using ::babylon::GarbageCollector;

// 定义一个GarbageCollector实例，模板参数是回收动作实例的类型
// 要求std::invocable(Reclaimer)
GarbageCollector<Reclaimer> garbage_collector;

// 调整设定异步队列长度，默认长度为1，一般都需要根据实际情况设置更长
// 异步队列放满后，retire动作会开始阻塞直到异步回收执行执行完成
garbage_collector.set_queue_capacity(...);

// 启动异步回收线程
garbage_collector.start();

// 执行淘汰动作，默认内部进行一次Epoch::tick并绑定reclaimer一同入队
// 批量淘汰时可以在外部统一进行一次Epoch::tick并将返回值传入lowest_epoch
// 淘汰动作不要求在epoch临界区内部执行，甚至鼓励在临界区外执行，避免阻塞时队列过短引起死锁
garbage_collector.retire(::std::move(reclaimer));
garbage_collector.retire(::std::move(reclaimer), lowest_epoch);

// 结束异步回收线程，会等待当前排队中的淘汰任务全部回收完成
garbage_collector.stop();
```
