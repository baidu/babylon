package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_TEST_COPTS')

cc_test(
  name = 'test_async_file_appender',
  srcs = ['test_async_file_appender.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:logging_async_file_appender',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_async_log_stream',
  srcs = ['test_async_log_stream.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:logging_async_log_stream',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_statically_initialize',
  srcs = ['test_statically_initialize.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:logging_logger',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_interface',
  srcs = ['test_interface.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:logging_interface',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_log_entry',
  srcs = ['test_log_entry.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:logging_log_entry',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_log_stream',
  srcs = ['test_log_stream.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:logging_log_stream',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_logger',
  srcs = ['test_logger.cpp'],
  copts = BABYLON_TEST_COPTS + ['-fno-access-control'],
  deps = [
    '//:logging_logger',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_log_statically',
  srcs = ['test_log_statically.cpp'],
  copts = BABYLON_TEST_COPTS,
  linkstatic = True,
  deps = [
    '//:logging_logger',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_rolling_file_object',
  srcs = ['test_rolling_file_object.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:logging_rolling_file_object',
    '@com_google_googletest//:gtest_main',
  ]
)
