package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_TEST_COPTS')

cc_test(
  name = 'test_aggregate',
  srcs = ['test_aggregate.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '//test/proto',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_array',
  srcs = ['test_array.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_compatible',
  srcs = ['test_compatible.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '//test/proto',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_list',
  srcs = ['test_list.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_map',
  srcs = ['test_map.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_message',
  srcs = ['test_message.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '//test/proto',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_scalar',
  srcs = ['test_scalar.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_serializer',
  srcs = ['test_serializer.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_set',
  srcs = ['test_set.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_shared_ptr',
  srcs = ['test_shared_ptr.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_string',
  srcs = ['test_string.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_traits',
  srcs = ['test_traits.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_unique_ptr',
  srcs = ['test_unique_ptr.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_vector',
  srcs = ['test_vector.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:serialization',
    '@com_google_googletest//:gtest_main',
  ]
)
