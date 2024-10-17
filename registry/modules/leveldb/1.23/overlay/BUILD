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
  name = 'leveldb',
  lib_source = ':all_srcs',
  cache_entries = {
    'LEVELDB_BUILD_TESTS': 'OFF',
    'LEVELDB_BUILD_BENCHMARKS': 'OFF',
    'CMAKE_CXX_FLAGS': '-I$$EXT_BUILD_DEPS$$/include',
    'CMAKE_EXE_LINKER_FLAGS': '-L$$EXT_BUILD_DEPS$$/lib',
  },
  postfix_script = SYMLINK2REAL_SCRIPT,
  lib_name = 'libleveldb',
  visibility = ['//visibility:public'],
  deps = [
    '@crc32c',
    '@snappy',
  ],
)
