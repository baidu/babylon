#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/releases/download/v1.4.0/v1.4.0.tar.gz
NAME=babylon-1.4.0
SHA256=0dd2f1690e7dd842406a4e6d55c507ae224aa1ebac1089d43891b5015f9df2fc
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
