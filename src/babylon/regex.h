#pragma once

#include "babylon/environment.h"

// See https://github.com/baidu/babylon/issues/68
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#if !__clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <regex>
#pragma GCC diagnostic pop
