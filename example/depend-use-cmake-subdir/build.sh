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

URL=https://github.com/abseil/abseil-cpp/archive/refs/tags/20230125.1.tar.gz
NAME=abseil-cpp-20230125.1
SHA256=81311c17599b3712069ded20cca09a62ab0bf2a89dfa16993786c8782b7ed145
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME abseil-cpp
tar xzf $NAME.tar.gz
mv $NAME abseil-cpp

URL=https://github.com/boostorg/boost/releases/download/boost-1.83.0/boost-1.83.0.tar.gz
NAME=boost-1.83.0
SHA256=0c6049764e80aa32754acd7d4f179fd5551d8172a83b71532ae093e7384e98da
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME boost
tar xzf $NAME.tar.gz
mv $NAME boost

URL=https://github.com/protocolbuffers/protobuf/archive/refs/tags/v22.5.tar.gz
NAME=protobuf-22.5
SHA256=4b98c800b352e7582bc92ed398999030ce4ebb49c7858dcb070850ec476b72f2
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME protobuf
tar xzf $NAME.tar.gz
mv $NAME protobuf

cmake -Bbuild
cmake --build build
