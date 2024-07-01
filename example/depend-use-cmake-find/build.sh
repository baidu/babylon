#!/bin/sh
set -ex

# sudo apt instal abseil-cpp
# sudo apt instal boost
# sudo apt instal protobuf

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.2.2.tar.gz
NAME=babylon-1.2.2
SHA256=241d0d3e8bf89c9b47765c794aa5153f73b32a7f419c48d8aeeeb4461cf8aec7
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
