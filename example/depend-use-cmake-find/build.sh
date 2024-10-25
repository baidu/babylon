#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/releases/download/v1.4.1/v1.4.1.tar.gz
NAME=babylon-1.4.1
SHA256=930e8d24822a472466e8b616011a57c37021b02486ad19ee7b62c12bfef923b8
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
