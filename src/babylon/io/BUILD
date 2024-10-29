package(
  default_visibility = ['//:__pkg__', '//src/babylon:__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'io',
  deps = [':entry', ':error', ':network_service'],
)

cc_library(
  name = 'entry',
  srcs = ['entry.cpp'],
  hdrs = ['entry.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon/reusable:page_allocator',
  ],
)

cc_library(
  name = 'error',
  srcs = ['error.cpp'],
  hdrs = ['error.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon:string_view',
  ],
)

cc_library(
  name = 'network_service',
  srcs = ['network_service.cpp'],
  hdrs = ['network_service.h'],
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    ':entry',
    ':error',
    '//:coroutine',
    '//:executor',
    '//:logging',
    '@liburing',
    '@com_google_absl//absl/strings:cord',
  ],
)
