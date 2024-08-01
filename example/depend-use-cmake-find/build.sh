#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.3.1.tar.gz
NAME=babylon-1.3.1
SHA256=4f059bfc6b57b7179df95e5b2a09d15625f421c69a22d8b1a96cbc9df7680cf3
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME babylon
tar xzf $NAME.tar.gz
mv $NAME babylon

cd babylon
cmake -Bbuild -DCMAKE_INSTALL_PREFIX=$PWD/output -DBUILD_TESTING=OFF
cmake --build build
cmake --install build
cd -

cmake -Bbuild -DCMAKE_PREFIX_PATH=babylon/output
cmake --build build
