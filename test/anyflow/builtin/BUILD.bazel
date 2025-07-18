package(
  default_visibility = ['//:__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_TEST_COPTS')

cc_test(
  name = 'test_const',
  srcs = ['test_const.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:anyflow',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_expression',
  srcs = ['test_expression.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:anyflow',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_select',
  srcs = ['test_select.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:anyflow',
    '@com_google_googletest//:gtest_main',
  ]
)
