package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'serialization',
  srcs = glob(['*.cpp']),
  hdrs = glob(['*.h', '*.hpp']),
  copts = BABYLON_COPTS,
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon:any',
    '//src/babylon/logging:logger',
    '@com_google_protobuf//:protobuf',
    '//:boost.preprocessor',
    '@com_google_absl//absl/container:flat_hash_map',
  ],
)
