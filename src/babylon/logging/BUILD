package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'logging',
  deps = [
    'async_log_stream',
    'logger',
    'rolling_file_object',
  ]
)

cc_library(
  name = 'async_file_appender',
  srcs = ['async_file_appender.cpp'],
  hdrs = ['async_file_appender.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':file_object',
    ':log_entry',
    '//src/babylon/reusable:page_allocator',
  ],
)

cc_library(
  name = 'async_log_stream',
  srcs = ['async_log_stream.cpp'],
  hdrs = ['async_log_stream.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':async_file_appender',
    ':logger',
  ],
)

cc_library(
  name = 'file_object',
  srcs = ['file_object.cpp'],
  hdrs = ['file_object.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon:environment',
  ],
)

cc_library(
  name = 'log_entry',
  srcs = ['log_entry.cpp'],
  hdrs = ['log_entry.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon/reusable:page_allocator',
  ],
)

cc_library(
  name = 'log_severity',
  srcs = ['log_severity.cpp'],
  hdrs = ['log_severity.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon:string_view',
  ],
)

cc_library(
  name = 'log_stream',
  srcs = ['log_stream.cpp'],
  hdrs = ['log_stream.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':log_severity',
    '//src/babylon:time',
    '//src/babylon:type_traits',
    '@com_google_absl//absl/strings:str_format',
  ],
)

cc_library(
  name = 'logger',
  srcs = ['logger.cpp', 'interface.cpp'],
  hdrs = ['logger.h', 'interface.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':log_stream',
    '//src/babylon/concurrent:thread_local',
    '//src/babylon/concurrent:transient_hash_table',
    '//src/babylon/reusable:page_allocator',
  ],
)

cc_library(
  name = 'rolling_file_object',
  srcs = ['rolling_file_object.cpp'],
  hdrs = ['rolling_file_object.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':file_object',
    '//src/babylon:string_view',
    '//src/babylon:time',
    '@com_google_absl//absl/time',
  ],
)
