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

alias(
  name = 'event',
  actual = 'libevent',
  visibility = ['//visibility:public'],
)

cmake(
  name = 'libevent',
  lib_source = ':all_srcs',
  cache_entries = {
    'EVENT__DISABLE_BENCHMARK': 'ON',
    'EVENT__DISABLE_TESTS': 'ON',
    'EVENT__DISABLE_SAMPLES': 'ON',
    'EVENT__LIBRARY_TYPE': 'STATIC',
    'OPENSSL_ROOT_DIR': '$$EXT_BUILD_DEPS$$/openssl',
  },
  postfix_script = SYMLINK2REAL_SCRIPT,
  out_static_libs = [
    'libevent.a',
    'libevent_core.a',
    'libevent_extra.a',
    'libevent_openssl.a',
    'libevent_pthreads.a',
  ],
  deps = [
    '@openssl',
  ],
  visibility = ['//visibility:public'],
)
