#include "babylon/time.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/time/time.h)
// clang-format on

BABYLON_NAMESPACE_BEGIN

// 基于缓存的快速地区时间转换器
//
// 核心原理基于『稳定地区时间段』这个概念完成
// 在不考虑夏令时的情况下，地区时间和国际时间仅存在一个稳定的时差
// 但是考虑了夏令时启用和禁用之后，这个稳定时差区域会拆散成一系列小区间
// 国际时间线：------------------------------------------------------->
// 地区时间线：---------------->                  -------------------->
// 地区时间线（夏令时）：       ----------------->
//
// 由于转换地区时间的核心场景是转换『当前时间』，而当前时间是缓慢自增的
// 因此基于记录下最后一次转换的时间，就可以通过秒数变化来局部更新
// 例如最典型的情况，关于时间的众多维度中，仅变更秒数字段即可
// 此时由于避开了闰年/闰月/星期等更新逻辑比较复杂的字段，可以显著提升性能
class FastLocalTimeConverter {
 public:
  // 入口函数，参数含义和localtime_r一致
  void convert(time_t time_point, struct ::tm& local) noexcept;

 private:
  // 计算星期并转换为tm::tm_wday
  static int to_weekday(::absl::CivilSecond civil) noexcept;
  // convert在缓存加速都不可用时的兜底实现
  void convert_fallback(time_t time_point, struct ::tm& local) noexcept;
  // 缓存覆盖范围移动到time_point所在的新稳定地区时间段
  void move_range(time_t time_point) noexcept;
  // 完整设置日期和时间缓存
  void set_cache(time_t cache_time, ::absl::CivilSecond cache_civil) noexcept;
  // 当前缓存结果输出
  void fill_time_struct(struct ::tm& local) noexcept;
  // 基于缓存增量更新
  void move_cache(time_t time_point) noexcept;
  // 超出时间范围，需要连带日期一起更新
  void move_cache_slow(time_t time_point) noexcept;

  // 当前稳定地区时间段，以及是否启用夏令时
  time_t _begin_time {::std::numeric_limits<time_t>::min()};
  time_t _end_time {::std::numeric_limits<time_t>::min()};
  bool _is_dst {false};
  int _offset {0};
  const char* _zone_abbr {nullptr};
  // 日期级别缓存
  time_t _cache_time {::std::numeric_limits<time_t>::min()};
  ::absl::CivilSecond _cache_civil;
  int _cache_weekday {::std::numeric_limits<int>::min()};
  int _cache_yearday {::std::numeric_limits<int>::min()};
  // 时间级别缓存
  time_t _fast_cache_time {::std::numeric_limits<time_t>::min()};
  time_t _cache_second {::std::numeric_limits<time_t>::min()};
  time_t _cache_minute {::std::numeric_limits<int>::min()};
  int _cache_hour {::std::numeric_limits<int>::min()};
  // 时区
  ::absl::TimeZone _zone {::absl::LocalTimeZone()};
};

////////////////////////////////////////////////////////////////////////////////
// FastLocalTimeConverter begin
void FastLocalTimeConverter::convert(time_t time_point,
                                     struct ::tm& local) noexcept {
  // 转换时间假设持续向前，回退场景直接交给兜底算法由absl完成
  if (ABSL_PREDICT_FALSE(time_point < _begin_time)) {
    convert_fallback(time_point, local);
    return;
  }

  // 时间经过了夏令时转换边界，切换到下一个连续区域
  if (ABSL_PREDICT_FALSE(time_point >= _end_time)) {
    move_range(time_point);
    fill_time_struct(local);
    return;
  }

  // 无夏令时或者在一个夏令时连续区域内，基于缓存做增量计算
  move_cache(time_point);
  fill_time_struct(local);
}

int FastLocalTimeConverter::to_weekday(::absl::CivilSecond civil) noexcept {
  switch (::absl::GetWeekday(::absl::CivilDay(civil))) {
    case ::absl::Weekday::sunday:
      return 0;
    case ::absl::Weekday::monday:
      return 1;
    case ::absl::Weekday::tuesday:
      return 2;
    case ::absl::Weekday::wednesday:
      return 3;
    case ::absl::Weekday::thursday:
      return 4;
    case ::absl::Weekday::friday:
      return 5;
    case ::absl::Weekday::saturday:
      return 6;
    default:
      ::abort();
  }
}

ABSL_ATTRIBUTE_NOINLINE
void FastLocalTimeConverter::convert_fallback(time_t time_point,
                                              struct ::tm& local) noexcept {
  auto time = ::absl::FromTimeT(time_point);
  auto ci = _zone.At(time);
  local.tm_sec = ci.cs.second();
  local.tm_min = ci.cs.minute();
  local.tm_hour = ci.cs.hour();
  local.tm_mday = ci.cs.day();
  local.tm_mon = ci.cs.month() - 1;
  local.tm_year = ci.cs.year() - 1900;
  local.tm_wday = to_weekday(ci.cs);
  local.tm_yday = ::absl::GetYearDay(::absl::CivilDay(ci.cs)) - 1;
  local.tm_isdst = ci.is_dst;
  local.tm_gmtoff = ci.offset;
  local.tm_zone = ci.zone_abbr;
}

ABSL_ATTRIBUTE_NOINLINE
void FastLocalTimeConverter::move_range(time_t time_point) noexcept {
  // 计算时间点对应的国际时间absl::Time和地区时间absl::CivilSecond
  auto time = ::absl::FromTimeT(time_point);
  auto ci = _zone.At(time);
  {
    // 寻找时间点向前的最近一个夏令时转换边界
    ::absl::TimeZone::CivilTransition trans;
    bool has_prev = _zone.PrevTransition(time, &trans);
    if (has_prev) {
      auto ti = _zone.At(trans.to);
      _begin_time = ::absl::ToTimeT(ti.trans);
    } else {
      _begin_time = ::std::numeric_limits<time_t>::min();
    }
  }
  {
    // 寻找时间点向后的最近一个夏令时转换边界
    ::absl::TimeZone::CivilTransition trans;
    bool has_next = _zone.NextTransition(time, &trans);
    if (has_next) {
      auto ti = _zone.At(trans.to);
      _end_time = ::absl::ToTimeT(ti.trans);
    } else {
      _end_time = ::std::numeric_limits<time_t>::max();
    }
  }

  // 记录当前连续区域是夏令时启用区域还是禁用区域
  _is_dst = ci.is_dst;
  _zone_abbr = ci.zone_abbr;
  _offset = ci.offset;
  // 成对记录国际时间和地区时间
  set_cache(time_point, ci.cs);
}

ABSL_ATTRIBUTE_NOINLINE
void FastLocalTimeConverter::set_cache(
    time_t cache_time, ::absl::CivilSecond cache_civil) noexcept {
  // 设置日期缓存
  _cache_time = cache_time;
  _cache_civil = cache_civil;
  // 额外设置星期
  _cache_weekday = to_weekday(_cache_civil);
  // 额外设置年内天数
  _cache_yearday = ::absl::GetYearDay(::absl::CivilDay(_cache_civil)) - 1;
  // 设置时间缓存
  _fast_cache_time = cache_time;
  _cache_second = cache_civil.second();
  _cache_minute = cache_civil.minute();
  _cache_hour = cache_civil.hour();
}

void FastLocalTimeConverter::fill_time_struct(struct ::tm& local) noexcept {
  local.tm_sec = _cache_second;
  local.tm_min = _cache_minute;
  local.tm_hour = _cache_hour;
  local.tm_mday = _cache_civil.day();
  local.tm_mon = _cache_civil.month() - 1;
  local.tm_year = _cache_civil.year() - 1900;
  local.tm_wday = _cache_weekday;
  local.tm_yday = _cache_yearday;
  local.tm_isdst = _is_dst;
  local.tm_gmtoff = _offset;
  local.tm_zone = _zone_abbr;
}

void FastLocalTimeConverter::move_cache(time_t time_point) noexcept {
  static constexpr int secs_per_min = 60;
  static constexpr int min_per_hour = 60;
  static constexpr int hour_per_day = 24;

  // 先将diff增加到秒
  auto diff = time_point - _fast_cache_time;
  auto cache_second = _cache_second;
  cache_second += diff;
  // 主要加速场景时间不会回退，回退发生时不尝试快速更新
  if (ABSL_PREDICT_FALSE(cache_second < 0)) {
    move_cache_slow(time_point);
    return;
  }
  // 如果结果不溢出，其余成分保持
  if (cache_second < secs_per_min) {
    _fast_cache_time = time_point;
    _cache_second = cache_second;
    return;
  }

  // 秒溢出后进位到分钟，如果结果不溢出，其余成分保持
  auto cache_minute = _cache_minute;
  cache_minute += cache_second / secs_per_min;
  cache_second %= secs_per_min;
  if (cache_minute < min_per_hour) {
    _fast_cache_time = time_point;
    _cache_second = cache_second;
    _cache_minute = cache_minute;
    return;
  }

  // 分钟溢出后进位到小时，如果结果不溢出，其余成分保持
  auto cache_hour = _cache_hour;
  cache_hour += cache_minute / min_per_hour;
  cache_minute %= min_per_hour;
  if (cache_hour < hour_per_day) {
    _fast_cache_time = time_point;
    _cache_second = cache_second;
    _cache_minute = cache_minute;
    _cache_hour = cache_hour;
    return;
  }

  // 小时溢出则进入日期变更领域
  move_cache_slow(time_point);
}

void FastLocalTimeConverter::move_cache_slow(time_t time_point) noexcept {
  auto cache_civil = _cache_civil + (time_point - _cache_time);
  set_cache(time_point, cache_civil);
}
// FastLocalTimeConverter end
////////////////////////////////////////////////////////////////////////////////

void localtime(const time_t* secs_since_epoch,
               struct ::tm* time_struct) noexcept {
  static thread_local FastLocalTimeConverter converter;
  converter.convert(*secs_since_epoch, *time_struct);
}

BABYLON_NAMESPACE_END
