#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/releases/download/v1.4.3/v1.4.3.tar.gz
NAME=babylon-1.4.3
SHA256=88c2b933a5d031ec7f528e27f75e3904f4a0c63aef8f9109170414914041d0ec
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
