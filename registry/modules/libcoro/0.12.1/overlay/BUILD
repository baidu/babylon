package(
  default_visibility = ['//:__pkg__'],
)

load('@rules_foreign_cc//foreign_cc:defs.bzl', 'cmake')

filegroup(
  name = 'all_srcs',
  srcs = glob(['**'], exclude = ['bazel-*/**']),
)

cmake(
  name = 'libcoro',
  lib_source = ':all_srcs',
  cache_entries = {
    'LIBCORO_BUILD_TESTS': 'OFF',
    'LIBCORO_BUILD_EXAMPLES': 'OFF',
    'LIBCORO_FEATURE_NETWORKING': 'OFF',
  },
  lib_name = 'libcoro',
  visibility = ['//visibility:public'],
)
