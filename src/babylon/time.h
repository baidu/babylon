#pragma once

#include "babylon/environment.h"

#include <time.h>

BABYLON_NAMESPACE_BEGIN

// localtime_r轻量级实现
// 
// 与localtime_r相比几个核心不同点
// 1. 仅首次调用时获取系统时区，不再每次调用执行tzset，避免全局锁
// 2. 针对localtime_r最典型的『当前』时间格式化场景，引入线程缓存加速
//
// 主要加速原理是假设时间是『小步幅前进式』变化的，通过缓存之前的计算结果
// 并结合每次的变化执行增量计算，可以大幅减少闰年，星期，夏令时等计算操作的频次
// 日期计算核心功能依赖absl::TimeZone完成
void localtime(const time_t* secs_since_epoch, struct ::tm* time_struct) noexcept;

BABYLON_NAMESPACE_END
