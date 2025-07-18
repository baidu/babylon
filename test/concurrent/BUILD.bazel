package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_TEST_COPTS')

cc_test(
  name = 'test_bounded_queue',
  srcs = ['test_bounded_queue.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_bounded_queue',
    '//:logging_interface',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_bounded_queue_press_mpmc',
  srcs = ['test_bounded_queue_press_mpmc.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_bounded_queue',
    '//:logging_interface',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_counter',
  srcs = ['test_counter.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_counter',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_deposit_box',
  srcs = ['test_deposit_box.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_deposit_box',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_epoch',
  srcs = ['test_epoch.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_epoch',
    '//:logging_logger',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_execution_queue',
  srcs = ['test_execution_queue.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_execution_queue',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_garbage_collector',
  srcs = ['test_garbage_collector.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_garbage_collector',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_id_allocator',
  srcs = ['test_id_allocator.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_id_allocator',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_object_pool',
  srcs = ['test_object_pool.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_object_pool',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_sched_interface',
  srcs = ['test_sched_interface.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_sched_interface',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_thread_local',
  srcs = ['test_thread_local.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_thread_local',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_transient_hash_table',
  srcs = ['test_transient_hash_table.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_transient_hash_table',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_transient_topic',
  srcs = ['test_transient_topic.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_transient_topic',
    '@com_google_googletest//:gtest_main',
  ]
)

cc_test(
  name = 'test_vector',
  srcs = ['test_vector.cpp'],
  copts = BABYLON_TEST_COPTS,
  deps = [
    '//:concurrent_vector',
    '@com_google_googletest//:gtest_main',
  ]
)
