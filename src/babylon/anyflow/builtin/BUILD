package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'builtin',
  srcs = glob(['*.cpp']),
  hdrs = glob(['*.h', '*.hpp']),
  strip_include_prefix = '//src',
  copts = BABYLON_COPTS,
  deps = [
    '//src/babylon/anyflow:engine',
    '@boost.spirit',
  ],
)
