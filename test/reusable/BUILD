package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_test(
  name = 'test_allocator',
  srcs = ['test_allocator.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_allocator',
    '//proto',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_manager',
  srcs = ['test_manager.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_manager',
    '//:reusable_string',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_memory_resource',
  srcs = ['test_memory_resource.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_memory_resource',
    '//proto',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_message',
  srcs = ['test_message.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_manager',
    '//:reusable_string',
    '//proto',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_page_allocator',
  srcs = ['test_page_allocator.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_page_allocator',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_string',
  srcs = ['test_string.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_string',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_traits',
  srcs = ['test_traits.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_traits',
    '//proto',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_vector',
  srcs = ['test_vector.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:reusable_vector',
    '//:reusable_string',
    '@googletest//:gtest_main',
  ]
)
