package(
  default_visibility = ['//visibility:public'],
)

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

################################################################################
# Protocol Buffer启用ArenaString的开关 --//:arenastring 
bool_flag(
  name = 'arenastring',
  build_setting_default = False,
)

config_setting(
  name = 'patch-protobuf',
  flag_values = {
    ':arenastring': 'True'
  },
)

alias(
  name = 'protoc',
  actual = select({
    ':patch-protobuf': '@com_google_protobuf_patch//:protoc',
    '//conditions:default': '@com_google_protobuf//:protoc',
  }),
)

alias(
  name = 'protobuf',
  actual = select({
    ':patch-protobuf': '@com_google_protobuf_patch//:protobuf',
    '//conditions:default': '@com_google_protobuf//:protobuf',
  }),
)

alias(
  name = 'cc_toolchain',
  actual = select({
    ':patch-protobuf': '@com_google_protobuf_patch//:cc_toolchain',
    '//conditions:default': '@com_google_protobuf//:cc_toolchain',
  }),
)
################################################################################

################################################################################
bool_flag(
  name = 'werror',
  build_setting_default = False,
)

config_setting(
  name = 'enable_werror',
  flag_values = {
    ':werror': 'True'
  },
)
################################################################################

################################################################################
# 编译器识别，用来支持不同编译器启用特定编译选项
config_setting(
  name = 'compiler_gcc',
  flag_values = {
    '@bazel_tools//tools/cpp:compiler' : 'gcc',
  },
)

config_setting(
  name = 'compiler_clang',
  flag_values = {
    '@bazel_tools//tools/cpp:compiler' : 'clang',
  },
)
################################################################################

################################################################################
# 聚合目标
alias(
  name = 'babylon',
  actual = '//src/babylon',
)
################################################################################

################################################################################
# 子模块目标
alias(
  name = 'any',
  actual = '//src/babylon:any',
)

alias(
  name = 'anyflow',
  actual = '//src/babylon/anyflow',
)

alias(
  name = 'concurrent',
  actual = '//src/babylon/concurrent',
)

alias(
  name = 'concurrent_bounded_queue',
  actual = '//src/babylon/concurrent:bounded_queue',
)

alias(
  name = 'concurrent_counter',
  actual = '//src/babylon/concurrent:counter',
)

alias(
  name = 'concurrent_id_allocator',
  actual = '//src/babylon/concurrent:id_allocator',
)

alias(
  name = 'concurrent_object_pool',
  actual = '//src/babylon/concurrent:object_pool',
)

alias(
  name = 'concurrent_sched_interface',
  actual = '//src/babylon/concurrent:sched_interface',
)

alias(
  name = 'concurrent_thread_local',
  actual = '//src/babylon/concurrent:thread_local',
)

alias(
  name = 'concurrent_transient_hash_table',
  actual = '//src/babylon/concurrent:transient_hash_table',
)

alias(
  name = 'concurrent_transient_topic',
  actual = '//src/babylon/concurrent:transient_topic',
)

alias(
  name = 'concurrent_vector',
  actual = '//src/babylon/concurrent:vector',
)

alias(
  name = 'executor',
  actual = '//src/babylon:executor',
)

alias(
  name = 'future',
  actual = '//src/babylon:future',
)

alias(
  name = 'logging',
  actual = '//src/babylon/logging',
)

alias(
  name = 'logging_async_file_appender',
  actual = '//src/babylon/logging:async_file_appender',
)

alias(
  name = 'logging_interface',
  actual = '//src/babylon/logging:interface',
)

alias(
  name = 'logging_log_stream',
  actual = '//src/babylon/logging:log_stream',
)

alias(
  name = 'move_only_function',
  actual = '//src/babylon:move_only_function',
)

alias(
  name = 'mlock',
  actual = '//src/babylon:mlock',
)

alias(
  name = 'reusable',
  actual = '//src/babylon/reusable',
)

alias(
  name = 'reusable_allocator',
  actual = '//src/babylon/reusable:allocator',
)

alias(
  name = 'reusable_manager',
  actual = '//src/babylon/reusable:manager',
)

alias(
  name = 'reusable_memory_resource',
  actual = '//src/babylon/reusable:memory_resource',
)

alias(
  name = 'reusable_page_allocator',
  actual = '//src/babylon/reusable:page_allocator',
)

alias(
  name = 'reusable_string',
  actual = '//src/babylon/reusable:string',
)

alias(
  name = 'reusable_traits',
  actual = '//src/babylon/reusable:traits',
)

alias(
  name = 'reusable_vector',
  actual = '//src/babylon/reusable:vector',
)

alias(
  name = 'serialization',
  actual = '//src/babylon:serialization',
)

alias(
  name = 'string',
  actual = '//src/babylon:string',
)

alias(
  name = 'string_view',
  actual = '//src/babylon:string_view',
)

alias(
  name = 'time',
  actual = '//src/babylon:time',
)

alias(
  name = 'type_traits',
  actual = '//src/babylon:type_traits',
)
################################################################################
