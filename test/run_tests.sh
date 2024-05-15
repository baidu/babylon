#!/bin/bash
set -ex

find bazel-out/k8-fastbuild/bin/test/ -name test_* | fgrep -v . | fgrep -v _objs | while read file; do
  if readelf -d $file | fgrep ld-linux-aarch64.so.1; then
    qemu-aarch64 -L /usr/aarch64-linux-gnu $file
  else
    $file
  fi
done

                #-E LD_LIBRARY_PATH=/usr/aarch64-linux-gnu/lib \
