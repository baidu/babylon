package(
  default_visibility = ['//src/babylon:__subpackages__', '//:__pkg__'],
)

load('//:copts.bzl', 'BABYLON_COPTS')

cc_library(
  name = 'coroutine',
  deps = [
    'cancelable', ':futex', ':promise', ':task', 'traits',
  ],
)

cc_library(
  name = 'cancelable',
  hdrs = ['cancelable.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':task',
    '//src/babylon/concurrent:deposit_box',
  ],
)

cc_library(
  name = 'futex',
  srcs = ['futex.cpp'],
  hdrs = ['futex.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':promise',
    '//src/babylon/concurrent:deposit_box',
  ],
)

cc_library(
  name = 'promise',
  hdrs = ['promise.h', 'future_awaitable.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':traits',
    '//src/babylon:basic_executor',
    '//src/babylon:future',
  ],
)

cc_library(
  name = 'task',
  hdrs = ['task.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    ':promise',
  ],
)

cc_library(
  name = 'traits',
  hdrs = ['traits.h'],
  copts = BABYLON_COPTS,
  includes = ['//src'],
  strip_include_prefix = '//src',
  deps = [
    '//src/babylon:type_traits',
  ],
)
