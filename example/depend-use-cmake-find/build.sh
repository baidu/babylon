#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/archive/6de305913d5e632a19798573ef110955cbdc4560.tar.gz
NAME=babylon-6de305913d5e632a19798573ef110955cbdc4560
SHA256=5ec4a3fc659d69983453abcbc66a2a03b57fe87774105fff76c90895a3a168d3
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
