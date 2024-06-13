cc_library(
  name = 'reusable_rpc_protocol',
  srcs = ['reusable_rpc_protocol.cpp', 'reusable_rpc_protocol.trick.cpp'],
  hdrs = ['reusable_rpc_protocol.h'],
  deps = [
    '@brpc//:brpc',
    '@babylon//:concurrent_object_pool',
    '@babylon//:concurrent_transient_hash_table',
    '@babylon//:reusable',
  ],
)

proto_library(
  name = 'echo_proto',
  srcs = ['echo.proto'],
)

cc_proto_library(
  name = 'cc_echo_proto',
  deps = [':echo_proto'],
)

cc_binary(
  name = 'client',
  srcs = ['client.cpp'],
  deps = [
    ':cc_echo_proto',
    '@brpc//:brpc',
    '@tcmalloc//tcmalloc',
  ],
)

cc_binary(
  name = 'server',
  srcs = ['server.cpp'],
  deps = [
    ':cc_echo_proto',
    ':reusable_rpc_protocol',
    '@brpc//:brpc',
    '@tcmalloc//tcmalloc',
  ],
)
