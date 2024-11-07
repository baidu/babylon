#!/bin/sh
set -ex

bazel build --registry=file:///home/oathdruid/src/babylon/registry --registry=https://bcr.bazel.build --registry=https://baidu.github.io/babylon/registry --compilation_mode=opt --cxxopt=-std=c++17 client server server_babylon
