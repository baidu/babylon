#!/bin/sh
set -ex

# sudo apt instal abseil-cpp
# sudo apt instal boost
# sudo apt instal protobuf

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.1.5.tar.gz
NAME=babylon-1.1.5
SHA256=a8d37251972a522b4c6f4d28ac6bf536444ff0e0c0e47eebff37aa75ca2a65a6
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
