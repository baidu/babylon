package(
  default_visibility = ['//:__subpackages__'],
)

proto_library(
  name = 'proto_deps',
  srcs = select({
    '//:patch-protobuf': ['mutable/arena_example.proto'],
    '//conditions:default': ['arena_example.proto'],
  }),
  strip_import_prefix = select({
    '//:patch-protobuf': '/proto/mutable',
    '//conditions:default': '/proto',
  }),
)

cc_proto_library(
  name = 'proto',
  deps = [':proto_deps'],
)
