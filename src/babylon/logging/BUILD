package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'logging',
  deps = [
    'async_file_appender',
    'interface',
    'log_stream',
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
    ':logger',
    '//src/babylon/reusable:page_allocator',
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
  name = 'interface',
  srcs = ['interface.cpp'],
  hdrs = ['interface.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':log_stream',
    ':logger',
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
  hdrs = ['log_stream.h', 'log_stream.hpp'],
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
  srcs = ['logger.cpp'],
  hdrs = ['logger.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':log_stream',
    '//src/babylon/concurrent:thread_local',
    '//src/babylon/concurrent:transient_hash_table',
    '//src/babylon/reusable:page_allocator',
  ],
)
