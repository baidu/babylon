#!/bin/sh
set -ex

bazel run --registry=https://bcr.bazel.build --registry=https://baidu.github.io/babylon/registry example
bazel run --registry=https://bcr.bazel.build --registry=https://baidu.github.io/babylon/registry anyflow_multi_nodes
