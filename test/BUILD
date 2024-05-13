package(
  default_visibility = [':__subpackages__'],
)

load('@rules_cuda//cuda:defs.bzl', 'cuda_library', 'requires_cuda')
load('//:copts.bzl', 'BABYLON_COPTS')

cc_test(
  name = 'test_any',
  srcs = ['test_any.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:any',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_executor',
  srcs = ['test_executor.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:executor',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_future',
  srcs = ['test_future.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:future',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_mlock',
  srcs = ['test_mlock.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:mlock',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_move_only_function',
  srcs = ['test_move_only_function.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:move_only_function',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_string',
  srcs = ['test_string.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:string',
    '@googletest//:gtest_main',
  ]
)

cuda_library(
  name = 'test_string_view_cuda',
  srcs = ['test_string_view.cu'],
  testonly = True,
  deps = [
    '//:string_view',
  ],
  target_compatible_with = requires_cuda(),
)

cc_test(
  name = 'test_string_view',
  srcs = ['test_string_view.cpp', 'test_string_view.h', 'test_string_view_in_lib.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:string_view',
    '@googletest//:gtest_main',
  ] + select({
    '@rules_cuda//cuda:is_enabled': [':test_string_view_cuda'],
    '//conditions:default': [],
  }),
)

cc_test(
  name = 'test_time',
  srcs = ['test_time.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:time',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_type_traits',
  srcs = ['test_type_traits.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:type_traits',
    '@googletest//:gtest_main',
  ]
)
