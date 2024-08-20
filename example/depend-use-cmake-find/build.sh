#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.3.2.tar.gz
NAME=babylon-1.3.2
SHA256=11b13bd89879e9f563dfc438a60f7d03724e2a476e750088c356b2eb6b73597e
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
