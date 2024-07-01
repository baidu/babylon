#!/bin/sh
set -ex

bazel run --enable_bzlmod=false example
