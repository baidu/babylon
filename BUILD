package(
  default_visibility = ['//visibility:public'],
)

load('@bazel_skylib//rules:common_settings.bzl', 'bool_flag')

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
  name = 'application_context',
  actual = '//src/babylon:application_context',
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
  name = 'concurrent_deposit_box',
  actual = '//src/babylon/concurrent:deposit_box',
)

alias(
  name = 'concurrent_epoch',
  actual = '//src/babylon/concurrent:epoch',
)

alias(
  name = 'concurrent_execution_queue',
  actual = '//src/babylon/concurrent:execution_queue',
)

alias(
  name = 'concurrent_garbage_collector',
  actual = '//src/babylon/concurrent:garbage_collector',
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
  name = 'coroutine',
  actual = '//src/babylon/coroutine',
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
  name = 'logging_async_log_stream',
  actual = '//src/babylon/logging:async_log_stream',
)

alias(
  name = 'logging_interface',
  actual = '//src/babylon/logging:logger',
)

alias(
  name = 'logging_log_entry',
  actual = '//src/babylon/logging:log_entry',
)

alias(
  name = 'logging_log_stream',
  actual = '//src/babylon/logging:log_stream',
)

alias(
  name = 'logging_logger',
  actual = '//src/babylon/logging:logger',
)

alias(
  name = 'logging_rolling_file_object',
  actual = '//src/babylon/logging:rolling_file_object',
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

################################################################################
# bzlmod and workspace use different boost bazel support repo
load('bazel/module_name.bzl', 'module_name')

alias(
  name = 'boost.preprocessor',
  actual = '@boost.preprocessor' if module_name() else '@boost//:preprocessor',
)

alias(
  name = 'boost.spirit',
  actual = '@boost.spirit' if module_name() else '@boost//:spirit',
)
################################################################################
