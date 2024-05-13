package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_test(
  name = 'test_bounded_queue',
  srcs = ['test_bounded_queue.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_bounded_queue',
    '//:logging_interface',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_bounded_queue_press_mpmc',
  srcs = ['test_bounded_queue_press_mpmc.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_bounded_queue',
    '//:logging_interface',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_counter',
  srcs = ['test_counter.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_counter',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_id_allocator',
  srcs = ['test_id_allocator.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_id_allocator',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_object_pool',
  srcs = ['test_object_pool.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_object_pool',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_sched_interface',
  srcs = ['test_sched_interface.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_sched_interface',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_thread_local',
  srcs = ['test_thread_local.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_thread_local',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_transient_hash_table',
  srcs = ['test_transient_hash_table.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_transient_hash_table',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_transient_topic',
  srcs = ['test_transient_topic.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_transient_topic',
    '@googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_vector',
  srcs = ['test_vector.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//:concurrent_vector',
    '@googletest//:gtest_main',
  ]
)
