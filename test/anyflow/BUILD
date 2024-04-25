package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_test(
  name = 'test_builder',
  srcs = ['test_builder.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:anyflow',
  ]
)

cc_test(
  name = 'test_channel',
  srcs = ['test_channel.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:anyflow',
  ]
)

cc_test(
  name = 'test_closure',
  srcs = ['test_closure.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:anyflow',
  ]
)

cc_test(
  name = 'test_data',
  srcs = ['test_data.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:anyflow',
  ]
)

cc_test(
  name = 'test_dependency',
  srcs = ['test_dependency.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:anyflow',
  ]
)

cc_test(
  name = 'test_processor',
  srcs = ['test_processor.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:anyflow',
    '//:reusable_string',
    '//:reusable_vector',
  ]
)
