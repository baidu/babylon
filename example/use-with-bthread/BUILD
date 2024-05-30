cc_library(
  name = 'butex_interface',
  hdrs = ['butex_interface.h'],
  deps = [
    '@babylon//:concurrent_sched_interface',
    '@brpc//:bthread',
  ],
)

cc_library(
  name = 'bthread_executor',
  hdrs = ['bthread_executor.h'],
  deps = [
    ':butex_interface',
    '@babylon//:executor',
    '@brpc//:bthread',
  ],
)

cc_binary(
  name = 'example',
  srcs = ['example.cpp'],
  deps = [
    ':butex_interface',
    ':bthread_executor',
    '@babylon//:future',
    '@babylon//:logging',
  ],
)
