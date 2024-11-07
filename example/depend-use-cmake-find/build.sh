#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/releases/download/v1.4.2/v1.4.2.tar.gz
NAME=babylon-1.4.2
SHA256=d60ee9cd86a777137bf021c8861e97438a69cc857659d5eb39af9e8464434cf1
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
