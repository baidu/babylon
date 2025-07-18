proto_library(
  name = 'search_proto',
  srcs = ['search.proto'],
)

cc_proto_library(
  name = 'cc_search_proto',
  deps = [':search_proto'],
)

cc_library(
  name = 'butex_interface',
  hdrs = ['butex_interface.h'],
  deps = [
    '@babylon',
    '@brpc',
  ],
)

cc_library(
  name = 'bthread_graph_executor',
  srcs = ['bthread_graph_executor.cpp'],
  hdrs = ['bthread_graph_executor.h'],
  deps = [
    ':butex_interface',
  ],
)

cc_library(
  name = 'graph_configurator',
  hdrs = ['graph_configurator.h'],
  deps = [
    ':bthread_graph_executor',
    '@yaml-cpp',
  ],
)

cc_library(
  name = 'processors',
  srcs = ['parse.cpp', 'rank.cpp', 'recall.cpp', 'response.cpp', 'user_profile.cpp'],
  deps = [
    ':cc_search_proto',
    '@babylon',
    '@yaml-cpp',
  ],
  # BABYLON_REGISTER_COMPONENT need alwayslink
  alwayslink = True,
)

cc_binary(
  name = 'client',
  srcs = ['client.cpp'],
  deps = [
    ':cc_search_proto',
    '@brpc',
  ],
)

cc_binary(
  name = 'server',
  srcs = ['server.cpp'],
  deps = [
    ':cc_search_proto',
    ':graph_configurator',
    ':processors',
    '@brpc',
  ],
)
