#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.4.0.tar.gz
NAME=babylon-1.4.0
SHA256=12722ca7ddd698dca118a522a220b12f870178fcad98b78f0f6604cd0903d292
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
