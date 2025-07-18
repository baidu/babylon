package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

cc_library(
  name = 'patch',
  srcs = glob(['arena.*']),
  copts = ['-fno-access-control'],
  deps = [
    '//src/babylon/reusable:memory_resource_without_patch',
  ],
)
