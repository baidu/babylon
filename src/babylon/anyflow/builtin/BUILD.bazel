package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'builtin',
  srcs = glob(['*.cpp']),
  hdrs = glob(['*.h', '*.hpp']),
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon/anyflow:engine',
    '//:boost.spirit',
  ],
)
