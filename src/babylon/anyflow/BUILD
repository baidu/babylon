package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'anyflow',
  deps = [
    'builtin',
    'engine',
  ],
)

alias(
  name = 'builtin',
  actual = '//src/babylon/anyflow/builtin',
)

cc_library(
  name = 'engine',
  srcs = glob(['*.cpp']),
  hdrs = glob(['*.h', '*.hpp']),
  strip_include_prefix = '//src',
  copts = BABYLON_COPTS,
  deps = [
    '//src/babylon:any',
    '//src/babylon/concurrent:transient_topic',
    '//src/babylon:executor',
    '//src/babylon/logging:interface',
    '//src/babylon:move_only_function',
    '//src/babylon/reusable:manager',
    '@boost.preprocessor',
    '@com_google_absl//absl/container:flat_hash_map',
    '@com_google_absl//absl/container:inlined_vector',
  ],
)
