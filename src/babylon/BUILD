package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

# 聚合目标
cc_library(
  name = 'babylon',
  deps = [
    ':any', ':executor', ':future', ':mlock',
    ':move_only_function', ':serialization',
    ':string_view', ':time', ':type_traits',
    '//src/babylon/anyflow', '//src/babylon/concurrent',
    '//src/babylon/logging', '//src/babylon/reusable',
  ],
)

cc_library(
  name = 'absl_base_internal_invoke',
  hdrs = ['absl_base_internal_invoke.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':environment',
    '@abseil-cpp//absl/base',
  ],
)

cc_library(
  name = 'absl_numeric_bits',
  hdrs = ['absl_numeric_bits.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':environment',
    '@abseil-cpp//absl/numeric:bits',
  ],
)

cc_library(
  name = 'absl_strings_internal_resize_uninitialized',
  hdrs = ['absl_strings_internal_resize_uninitialized.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':environment',
    '@abseil-cpp//absl/strings',
  ],
)

cc_library(
  name = 'any',
  srcs = ['any.cpp'],
  hdrs = ['any.h', 'any.hpp'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':type_traits',
  ],
)

cc_library(
  name = 'environment',
  hdrs = ['environment.h', 'protect.h', 'unprotect.h'],
  strip_include_prefix = '//src',
  copts = BABYLON_COPTS,
  deps = [
    '@abseil-cpp//absl/base:core_headers',
  ],
)

cc_library(
  name = 'executor',
  srcs = ['executor.cpp'],
  hdrs = ['executor.h', 'executor.hpp'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':future',
    '//src/babylon/concurrent:bounded_queue',
  ],
)

cc_library(
  name = 'future',
  hdrs = ['future.h', 'future.hpp'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':move_only_function',
    '//src/babylon/concurrent:sched_interface',
  ],
)

cc_library(
  name = 'mlock',
  srcs = ['mlock.cpp'],
  hdrs = ['mlock.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':string_view',
    '//src/babylon/logging:interface',
    '@abseil-cpp//absl/strings',
  ],
)

cc_library(
  name = 'move_only_function',
  hdrs = ['move_only_function.h', 'move_only_function.hpp'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':type_traits',
  ],
)

cc_library(
  name = 'new',
  srcs = ['new.cpp'],
  hdrs = ['new.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':environment',
  ],
)

cc_library(
  name = 'sanitizer_helper',
  hdrs = ['sanitizer_helper.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':environment',
  ],
)

cc_library(
  name = 'serialization',
  hdrs = ['serialization.h'],
  strip_include_prefix = '//src',
  copts = BABYLON_COPTS,
  deps = [
    '//src/babylon/serialization',
  ],
)

cc_library(
  name = 'string',
  hdrs = ['string.h', 'string.hpp'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':absl_strings_internal_resize_uninitialized',
    ':type_traits',
  ],
)

cc_library(
  name = 'string_view',
  hdrs = ['string_view.h', 'string_view.hpp'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':environment',
  ],
)

cc_library(
  name = 'time',
  srcs = ['time.cpp'],
  hdrs = ['time.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':environment',
    '@abseil-cpp//absl/time',
  ],
)

cc_library(
  name = 'type_traits',
  hdrs = ['type_traits.h', 'type_traits.hpp'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':absl_base_internal_invoke',
    ':string_view',
  ],
)
