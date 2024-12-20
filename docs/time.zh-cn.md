**[[English]](time.en.md)**

# time

## 原理

服务器在进行时间格式化时（最常见的场景就是日志格式化），经常需要使用到localtime_r函数，而localtime_r受限于posix标准，内部必须每次产生tzset行为，而tzset会检查时区设置文件变动，以及产生一个全局锁行为；这些动作都会导致它不适用于大并发场景，设置TZ环境变量可以规避检测文件动作，但是全局锁带来的影响仍然无法避免；

典型的工业级日志系统一般都会通过替换实现来彻底解决并发问题，可用已知的方案有

- absl::TimeZone也就是google/cctz
- apollo::cyber::common::LocalTime

这几个解决方案都完成了全局锁的去除，absl的实现比较系统，属于localtime_r的全功能版本；而comlog和apollo的实现在闰年计算和夏令时支持上做了一定简化取舍，相对全功能版本进一步优化了性能

这里提出一个基于缓存的新优化机制，实现了一个保持全功能的前提下，延续并进一步优化最常见的日志格式化领域的实用性能；由于自然时间大多情况下是逐步缓慢增长的，相对复杂的闰年星期等计算在增量模式的大多数情况下不会触发，可以节省大量的计算开销；

![](images/local_time.png)

## 使用方法

```c++
#include "babylon/time.h"

time_t time = ...  // 获取到一个timestamp

tm local;
babylon::localtime(&time, &local);
// 与localtime_r(&time, &local)效果一致，除了
// - 每个进程只会加载一次时区文件，运行时修改系统时区不会进行热加载
```

## 功能/性能对比

|             | 时区热切换 |   闰年支持  |  夏令时支持  | 星期支持 | 单线程性能 | 四线程性能 |
|-------------|:----------:|:-----------:|:------------:|:--------:|:----------:|:----------:|
| localtime_r |      ✓     |      ✓      |       ✓      |     ✓    |    282ns   |   2061ns   |
| absl        |   不支持   |      ✓      |       ✓      |     ✓    |    91ns    |    92ns    |
| apollo      |   不支持   | 1901 ~ 2099 |    不支持    |  不支持  |    11ns    |    11ns    |
| babylon     |   不支持   |      ✓      |       ✓      |     ✓    |     7ns    |     7ns    |
