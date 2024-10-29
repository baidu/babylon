package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_TEST_COPTS')

cc_test(
  name = 'test_io',
  srcs = ['test_io.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:io',
    '//test/proto',
    '@brpc',
    '@tcmalloc//tcmalloc',
    '@com_google_googletest//:gtest_main',
  ]
)
