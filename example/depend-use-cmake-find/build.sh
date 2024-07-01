#!/bin/sh
set -ex

# sudo apt instal abseil-cpp
# sudo apt instal boost
# sudo apt instal protobuf

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.2.1.tar.gz
NAME=babylon-1.2.1
SHA256=f5b200d1190d00d9d74519174b38d45a816cd3489a038696c3ae3885ba56d151
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
