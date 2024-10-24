package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

# 聚合目标
cc_library(
  name = 'babylon',
  deps = [
    ':any', ':application_context', ':executor', ':future', ':mlock',
    ':move_only_function', ':serialization',
    ':string_view', ':time', ':type_traits',
    '//src/babylon/anyflow', '//src/babylon/concurrent',
    '//src/babylon/coroutine', '//src/babylon/logging',
    '//src/babylon/reusable',
  ],
)

cc_library(
  name = 'absl_base_internal_invoke',
  hdrs = ['absl_base_internal_invoke.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':environment',
    '@com_google_absl//absl/base',
  ],
)

cc_library(
  name = 'absl_numeric_bits',
  hdrs = ['absl_numeric_bits.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':environment',
    '@com_google_absl//absl/numeric:int128',
  ],
)

cc_library(
  name = 'any',
  srcs = ['any.cpp'],
  hdrs = ['any.h', 'any.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':type_traits',
  ],
)

cc_library(
  name = 'application_context',
  srcs = ['application_context.cpp'],
  hdrs = ['application_context.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':any',
    '//src/babylon/logging',
    '//:boost.preprocessor',
    '@com_google_absl//absl/container:flat_hash_map',
  ],
)

cc_library(
  name = 'basic_executor',
  srcs = ['basic_executor.cpp'],
  hdrs = ['basic_executor.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':move_only_function',
  ],
)

cc_library(
  name = 'executor',
  srcs = ['executor.cpp'],
  hdrs = ['executor.h', 'executor.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':future',
    '//src/babylon/coroutine:task',
    '//src/babylon/concurrent:bounded_queue',
    '//src/babylon/concurrent:thread_local',
    '@com_google_absl//absl/types:optional',
  ],
)

cc_library(
  name = 'environment',
  hdrs = ['environment.h', 'protect.h', 'unprotect.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    '@com_google_absl//absl/base:core_headers',
  ],
)

cc_library(
  name = 'future',
  hdrs = ['future.h', 'future.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
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
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':string_view',
    '//src/babylon/logging:logger',
    '@com_google_absl//absl/strings',
  ],
)

cc_library(
  name = 'move_only_function',
  hdrs = ['move_only_function.h', 'move_only_function.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
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
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':environment',
  ],
)

cc_library(
  name = 'regex',
  hdrs = ['regex.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':environment',
  ],
)

cc_library(
  name = 'sanitizer_helper',
  hdrs = ['sanitizer_helper.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':environment',
  ],
)

cc_library(
  name = 'serialization',
  hdrs = ['serialization.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon/serialization',
  ],
)

cc_library(
  name = 'string',
  hdrs = ['string.h', 'string.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':type_traits',
    '@com_google_absl//absl/strings',
  ],
)

cc_library(
  name = 'string_view',
  hdrs = ['string_view.h', 'string_view.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
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
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':environment',
    '@com_google_absl//absl/time',
  ],
)

cc_library(
  name = 'type_traits',
  hdrs = ['type_traits.h', 'type_traits.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':absl_base_internal_invoke',
    ':string_view',
    '@com_google_absl//absl/utility',
  ],
)
