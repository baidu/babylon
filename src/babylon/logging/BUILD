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
    '//src/babylon/reusable:page_allocator',
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
    '//src/babylon:type_traits',
    '@com_google_absl//absl/strings:str_format',
  ],
)
