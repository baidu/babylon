package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

# 聚合目标
cc_library(
  name = 'concurrent',
  deps = [
    ':bounded_queue', ':counter', ':id_allocator',
    ':object_pool', ':sched_interface',
    ':thread_local', ':transient_hash_table',
    ':transient_topic', ':vector',
  ]
)

cc_library(
  name = 'bounded_queue',
  hdrs = ['bounded_queue.h', 'bounded_queue.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':sched_interface',
    '//src/babylon:absl_numeric_bits',
    '//src/babylon:new',
    '//src/babylon:type_traits',
    '@com_google_absl//absl/time',
  ],
)

cc_library(
  name = 'counter',
  srcs = ['counter.cpp'],
  hdrs = ['counter.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':thread_local',
  ],
)

cc_library(
  name = 'id_allocator',
  hdrs = ['id_allocator.h', 'id_allocator.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':vector',
  ],
)

cc_library(
  name = 'object_pool',
  hdrs = ['object_pool.h', 'object_pool.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':bounded_queue',
    '//src/babylon:move_only_function',
  ],
)

cc_library(
  name = 'sched_interface',
  hdrs = ['sched_interface.h', 'sched_interface.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon:absl_numeric_bits',
  ],
)

cc_library(
  name = 'thread_local',
  hdrs = ['thread_local.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':id_allocator',
  ],
)

cc_library(
  name = 'transient_hash_table',
  srcs = ['transient_hash_table.cpp'],
  hdrs = ['transient_hash_table.h', 'transient_hash_table.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':counter',
    '//src/babylon/reusable:allocator',
    '//src/babylon:absl_numeric_bits',
  ],
)

cc_library(
  name = 'transient_topic',
  hdrs = ['transient_topic.h', 'transient_topic.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':sched_interface',
    ':vector',
  ],
)

cc_library(
  name = 'vector',
  hdrs = ['vector.h', 'vector.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon:new',
    '//src/babylon:type_traits',
  ],
)
