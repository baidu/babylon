#include "babylon/logging/log_severity.h"

BABYLON_NAMESPACE_BEGIN

// 当库本身用-std=c++14编译时，强制产出的符号为常量符号
// 会和使用者采用-std=c++17编译时产生的唯一符号发生重定义冲突
// 这里对-std=c++14的情况改写为弱符号
#if __cplusplus < 201703L
ABSL_ATTRIBUTE_WEAK constexpr StringView LogSeverity::names[LogSeverity::NUM];
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END
