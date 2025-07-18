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

URL=https://github.com/fmtlib/fmt/archive/refs/tags/8.1.1.tar.gz
NAME=fmt-8.1.1
SHA256=3d794d3cf67633b34b2771eb9f073bde87e846e0d395d254df7b211ef1ec7346
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME fmt
tar xzf $NAME.tar.gz
mv $NAME fmt

cmake -Bbuild
cmake --build build
