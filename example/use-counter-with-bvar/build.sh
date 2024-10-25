#!/bin/sh
set -ex

bazel build --registry=https://bcr.bazel.build --registry=https://baidu.github.io/babylon/registry --compilation_mode=opt --cxxopt=-std=c++17 example
