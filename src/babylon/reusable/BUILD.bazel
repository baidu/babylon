package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'reusable',
  deps = [
    ':allocator', ':manager', ':memory_resource',
    ':page_allocator', ':string', ':traits', ':vector',
  ],
)

cc_library(
  name = 'allocator',
  hdrs = ['allocator.h', 'allocator.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':memory_resource',
    '//src/babylon/reusable/patch',
  ],
)

cc_library(
  name = 'manager',
  hdrs = ['manager.h', 'manager.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':traits',
  ],
)

cc_library(
  name = 'memory_resource',
  deps = [
    ':memory_resource_without_patch',
    '//src/babylon/reusable/patch',
  ],
)

cc_library(
  name = 'memory_resource_without_patch',
  srcs = ['memory_resource.cpp'],
  hdrs = ['memory_resource.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':page_allocator',
    '//src/babylon:sanitizer_helper',
    '@com_google_protobuf//:protobuf',
    '@com_google_absl//absl/utility',
  ],
)

cc_library(
  name = 'message_trick',
  srcs = ['message.trick.cpp'],
  hdrs = ['traits.h', 'message.h'],
  copts = BABYLON_COPTS + ['-fno-access-control'],
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':allocator',
    '//src/babylon:string',
  ],
)

cc_library(
  name = 'page_allocator',
  srcs = ['page_allocator.cpp'],
  hdrs = ['page_allocator.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon/concurrent:bounded_queue',
    '//src/babylon/concurrent:counter',
  ],
)

cc_library(
  name = 'string',
  hdrs = ['string.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':traits',
    '//src/babylon:serialization',
  ],
)

cc_library(
  name = 'traits',
  deps = [
    ':traits_without_trick',
    ':message_trick',
  ],
)

cc_library(
  name = 'traits_without_trick',
  srcs = ['message.cpp'],
  hdrs = ['traits.h', 'message.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':allocator',
    '//src/babylon:string',
  ],
)

cc_library(
  name = 'vector',
  hdrs = ['vector.h', 'vector.hpp'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':traits',
    '//src/babylon:serialization',
  ],
)
