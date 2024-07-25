#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.3.0.tar.gz
NAME=babylon-1.3.0
SHA256=4a8582db91e1931942555400096371586d0cf06ecaac0841aca652ef6d77aef0
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
