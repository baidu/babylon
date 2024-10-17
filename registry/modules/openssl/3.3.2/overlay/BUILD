package(
  default_visibility = ['//:__pkg__'],
)

load('@rules_foreign_cc//foreign_cc:defs.bzl', 'configure_make')

filegroup(
  name = 'all_srcs',
  srcs = glob(['**'], exclude = ['bazel-*/**']),
)

CONFIGURE_OPTIONS = [
  'no-shared',
  'zlib',
  '--with-zlib-include=$$EXT_BUILD_DEPS$$/include',
  '--with-zlib-lib=$$EXT_BUILD_DEPS$$/lib',
]

alias(
  name = 'ssl',
  actual = 'openssl',
  visibility = ['//visibility:public'],
)

alias(
  name = 'crypto',
  actual = 'openssl',
  visibility = ['//visibility:public'],
)

configure_make(
  name = 'openssl',
  lib_source = ':all_srcs',
  configure_command = 'Configure',
  configure_options = CONFIGURE_OPTIONS,
  targets = ['install_dev'],
  out_lib_dir = 'lib64',
  out_static_libs = [
    'libssl.a',
    'libcrypto.a',
  ],
  deps = [
    '@zlib',
  ],
  visibility = ['//visibility:public'],
)
