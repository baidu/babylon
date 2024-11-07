cc_library(
  name = 'swiss_message_factory',
  srcs = ['swiss_message_factory.cpp'],
  hdrs = ['swiss_message_factory.h'],
  deps = [
    '@brpc//:brpc',
    '@babylon//:concurrent_object_pool',
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
    '@brpc//:brpc',
    '@tcmalloc//tcmalloc',
  ],
)

cc_binary(
  name = 'server_babylon',
  srcs = ['server.cpp'],
  copts = ['-DWITH_BABYLON=1'],
  deps = [
    ':cc_echo_proto',
    ':swiss_message_factory',
    '@brpc//:brpc',
    '@tcmalloc//tcmalloc',
  ],
)
