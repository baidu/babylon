**[[English]](counter.en.md)**

# counter

## 原理

基于thread_local可以实现特化支持写多读少的高并发的计数器，每个线程对计数器的操作独立在TLS中生效，在读操作发生时，才会遍历收集得到最终结果；通过TLS的隔离，可以有效缓解计数器操作同一个内存区域带来的竞争

## 用法示例

```c++
#include <babylon/concurrent/counter.h>

using babylon::ConcurrentAdder;
using babylon::ConcurrentMaxer;
using babylon::ConcurrentSummer;

// 构造一个累加器，累加结果为有符号64位数，初始值为0
ConcurrentAdder var;
// 可以进行多线程加减累加
thread1:
    var << 100;
thread2:
    var << -20;
// 获取当前累加结果
var.value(); // == 80

// 构造一个最大值记录器，结果为有符号64位数
ConcurrentMaxer var;
// 获取记录的最大值，周期内无记录时返回0
ssize_t v = var.value();
// 获取记录的最大值，周期内无记录时返回false
var.value(v);
// 可以进行多线程记录
thread1:
    var << 100;
thread2:
    var << -20;
// 获取当前记录的最大值
var.value(); // == 100
// 清理记录结果，开启下一次记录周期
var.reset();
var << 50;
var.value(); // == 50

// 构造一个求和器，除了基础累加功能，还可以用于计算平均值
// 结果为有符号64位数，默认为0
ConcurrentMaxer var;
// 可以进行多线程求和
thread1:
    var << 100;
thread2:
    var << -20;
// 获取当前求和
var.value(); // {sum: 80, num: 2}
```
