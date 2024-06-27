cc_library(
  name = 'recorder',
  srcs = ['recorder.cpp', 'recorder.trick.cpp'],
  hdrs = ['recorder.h'],
  copts = ['-fno-access-control'],
  deps = [
    '@brpc//:bvar',
    '@babylon//:concurrent_counter',
  ],
)

cc_binary(
  name = 'example',
  srcs = ['example.cpp'],
  deps = [
    ':recorder',
    '@brpc',
    '@tcmalloc//tcmalloc',
  ],
)
