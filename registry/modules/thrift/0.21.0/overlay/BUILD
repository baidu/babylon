package(
  default_visibility = ['//:__pkg__'],
)

load('@rules_foreign_cc//foreign_cc:defs.bzl', 'cmake')

filegroup(
  name = 'all_srcs',
  srcs = glob(['**'], exclude = ['bazel-*/**']),
)

SYMLINK2REAL_SCRIPT = """bash -c '
set -ex
shopt -s globstar
for file in $INSTALLDIR/** ; do
  if [ -L $file ]; then
    target=`readlink -f $file`
    echo $file $target
    rm $file
    cp $target $file
  fi
done
'"""

cmake(
  name = 'thrift',
  lib_source = ':all_srcs',
  build_args = ['-j'],
  cache_entries = {
    'BUILD_COMPILER': 'OFF',
    'BUILD_TESTING': 'OFF',
    'BUILD_TUTORIALS': 'OFF',
    'WITH_AS3': 'OFF',
    'WITH_C_GLIB': 'OFF',
    'WITH_JAVA': 'OFF',
    'WITH_JAVASCRIPT': 'OFF',
    'WITH_NODEJS': 'OFF',
    'WITH_PYTHON': 'OFF',
    'WITH_OPENSSL': 'ON',
    'BUILD_SHARED_LIBS': 'OFF',
    'CMAKE_VERBOSE_MAKEFILE': 'ON',
    'LIBEVENT_ROOT': '$$EXT_BUILD_DEPS$$/libevent',
    'OPENSSL_ROOT_DIR': '$$EXT_BUILD_DEPS$$/openssl',
  },
  postfix_script = SYMLINK2REAL_SCRIPT,
  out_static_libs = [
    'libthrift.a',
    'libthriftnb.a',
    'libthriftz.a',
  ],
  visibility = ['//visibility:public'],
  deps = [
    '@boost.algorithm',
    '@boost.numeric_conversion',
    '@boost.smart_ptr',
    '@boost.tokenizer',
    '@boost.uuid',
    '@libevent',
    '@openssl',
    '@zlib',
  ],
)
