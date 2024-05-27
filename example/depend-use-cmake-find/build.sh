#!/bin/sh
set -ex

# sudo apt instal abseil-cpp
# sudo apt instal boost
# sudo apt instal protobuf

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.1.6.tar.gz
NAME=babylon-1.1.6
SHA256=a5bbc29f55819c90e00b40f9b5d2716d5f0232a158d69c530d8c7bac5bd794b6
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
