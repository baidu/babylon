package(
  default_visibility = [':__subpackages__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_test(
  name = 'test_bounded_queue',
  srcs = ['test_bounded_queue.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_bounded_queue',
    '//:logging_interface',
  ]
)

cc_test(
  name = 'test_bounded_queue_press_mpmc',
  srcs = ['test_bounded_queue_press_mpmc.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_bounded_queue',
    '//:logging_interface',
  ]
)

cc_test(
  name = 'test_counter',
  srcs = ['test_counter.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_counter',
  ]
)

cc_test(
  name = 'test_id_allocator',
  srcs = ['test_id_allocator.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_id_allocator',
  ]
)

cc_test(
  name = 'test_object_pool',
  srcs = ['test_object_pool.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_object_pool',
  ]
)

cc_test(
  name = 'test_sched_interface',
  srcs = ['test_sched_interface.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_sched_interface',
  ]
)

cc_test(
  name = 'test_thread_local',
  srcs = ['test_thread_local.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_thread_local',
  ]
)

cc_test(
  name = 'test_transient_hash_table',
  srcs = ['test_transient_hash_table.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_transient_hash_table',
  ]
)

cc_test(
  name = 'test_transient_topic',
  srcs = ['test_transient_topic.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_transient_topic',
  ]
)

cc_test(
  name = 'test_vector',
  srcs = ['test_vector.cpp'],
  copts = BABYLON_COPTS,
  deps = [
    '//test:main',
    '//:concurrent_vector',
  ]
)
