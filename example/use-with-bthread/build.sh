#!/bin/sh
set -ex

bazel run example
bazel run anyflow_multi_nodes
