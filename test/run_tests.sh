#!/bin/bash
set -ex

# SwissMemoryResource's patch obey odr rule. But still keep same struct with same defination and same size
# change detect_odr_violation to level 1 to check that
ASAN_OPTIONS=$ASAN_OPTIONS:detect_odr_violation=1
find bazel-out/k8-fastbuild/bin/test/ -name test_* | fgrep -v . | fgrep -v _objs | while read file; do
  if file $file | fgrep ld-linux-aarch64.so.1; then
    # lsan is not work well in qemu
    ASAN_OPTIONS=$ASAN_OPTIONS:detect_leaks=0 \
    qemu-aarch64 -L /usr/aarch64-linux-gnu $file
  else
    $file
  fi
done
