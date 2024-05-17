#!/bin/sh
set -ex

URL=https://github.com/baidu/babylon/archive/refs/tags/v1.1.4.tar.gz
NAME=babylon-1.1.4
SHA256=2e7efea3f0a8aeffc03f908ff2875a82eb5a94cd1de3e591b6dc388f7a992411
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME babylon
tar xzf $NAME.tar.gz
mv $NAME babylon

URL=https://github.com/abseil/abseil-cpp/archive/refs/tags/20230802.1.tar.gz
NAME=abseil-cpp-20230802.1
SHA256=987ce98f02eefbaf930d6e38ab16aa05737234d7afbab2d5c4ea7adbe50c28ed
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME abseil-cpp
tar xzf $NAME.tar.gz
mv $NAME abseil-cpp

URL=https://github.com/protocolbuffers/protobuf/archive/refs/tags/v25.3.tar.gz
NAME=protobuf-25.3
SHA256=d19643d265b978383352b3143f04c0641eea75a75235c111cc01a1350173180e
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME protobuf
tar xzf $NAME.tar.gz
mv $NAME protobuf

URL=https://github.com/boostorg/boost/releases/download/boost-1.83.0/boost-1.83.0.tar.gz
NAME=boost-1.83.0
SHA256=0c6049764e80aa32754acd7d4f179fd5551d8172a83b71532ae093e7384e98da
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME boost
tar xzf $NAME.tar.gz
mv $NAME boost

URL=https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
NAME=googletest-1.14.0
SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
if ! echo "$SHA256 $NAME.tar.gz" | sha256sum -c; then
  wget $URL --continue -O $NAME.tar.gz
fi
rm -rf $NAME googletest
tar xzf $NAME.tar.gz
mv $NAME googletest

cmake -Bbuild
cmake --build build
