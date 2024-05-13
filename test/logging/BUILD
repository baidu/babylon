package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_test(
  name = 'test_async_file_appender',
  srcs = ['test_async_file_appender.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:logging_async_file_appender',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_custom_default_provider',
  srcs = ['test_custom_default_provider.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:logging_interface',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_interface',
  srcs = ['test_interface.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:logging_interface',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_log_stream',
  srcs = ['test_log_stream.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:logging_log_stream',
    '@googletest//:gtest_main',
  ]
)
