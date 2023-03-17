#!/bin/bash

function configure() {
  #CC=afl-clang-lto CXX=afl-clang-lto++ RANLIB=llvm-ranlib AR=llvm-ar cmake -DCMAKE_BUILD_TYPE=Release ../
  CC=afl-clang-fast CXX=afl-clang-fast++ cmake -DCMAKE_BUILD_TYPE=Release ../
}

function build() {
  cmake --build . --config Release
}


export AFL_LLVM_ALLOWLIST=$(realpath allowlist.txt)
echo "AFL_LLVM_ALLOWLIST=$AFL_LLVM_ALLOWLIST"

mkdir -p build
pushd build

configure && build

popd
