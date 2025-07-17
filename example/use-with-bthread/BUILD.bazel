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

cc_library(
  name = 'bthread_graph_executor',
  srcs = ['bthread_graph_executor.cpp'],
  hdrs = ['bthread_graph_executor.h'],
  deps = [
    ':butex_interface',
    '@babylon//:anyflow',
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

cc_binary(
  name = 'anyflow_multi_nodes',
  srcs = ['anyflow_multi_nodes.cpp'],
  deps = [
    ':bthread_graph_executor',
    '@babylon//:anyflow',
  ],
)
