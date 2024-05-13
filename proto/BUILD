package(
  default_visibility = ['//test:__subpackages__'],
)

################################################################################
# Protocol Buffer启用ArenaString的开关 --//:arenastring
load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

bool_flag(
  name = 'arenastring',
  build_setting_default = False,
)

config_setting(
  name = 'use-arenastring',
  flag_values = {
    ':arenastring': 'True'
  },
)
################################################################################

proto_library(
  name = 'proto_deps',
  srcs = select({
    ':use-arenastring': ['mutable/arena_example.proto'],
    '//conditions:default': ['arena_example.proto'],
  }),
  strip_import_prefix = select({
    ':use-arenastring': '/proto/mutable',
    '//conditions:default': '/proto',
  }),
)

cc_proto_library(
  name = 'proto',
  testonly = True,
  deps = [':proto_deps'],
)
