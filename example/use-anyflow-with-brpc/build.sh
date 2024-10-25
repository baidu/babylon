#!/bin/sh
set -ex

bazel build --registry=https://bcr.bazel.build --registry=https://baidu.github.io/babylon/registry server client
