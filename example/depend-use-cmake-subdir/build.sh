#!/bin/sh
set -ex

cmake -Bbuild
cmake --build build
