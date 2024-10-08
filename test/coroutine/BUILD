package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_TEST_COPTS')

cc_test(
  name = 'test_task',
  srcs = ['test_task.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:executor',
    '@com_google_googletest//:gtest_main',
    '@libcoro',
  ]
)

cc_test(
  name = 'test_cancelable',
  srcs = ['test_cancelable.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:coroutine',
    '//:executor',
    '@com_google_googletest//:gtest_main',
    '@libcoro',
  ]
)
