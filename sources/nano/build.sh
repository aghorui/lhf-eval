#!/bin/bash
set -o errexit
set -o nounset
set -o xtrace

export LLVM_COMPILER="clang"
export LLVM_BITCODE_GENERATION_FLAGS="-fno-discard-value-names -O0 -Xclang \
                                      -disable-O0-optnone -g"

export LLVM_CC_NAME="clang-14"
export LLVM_CXX_NAME="clang++-14"
export LLVM_LINK_NAME="llvm-link-14"
export LLVM_AR_NAM="llvm-ar-14"

SRC_PATH="./nano-7.2"

cd "$SRC_PATH"

mkdir -p "build"
cd build

if [ -e 'Makefile' ]; then
    make clean
fi

CC=wllvm CXX=wllvm++ ../configure CFLAGS="-O0 -g"

make -j4 CC=wllvm CXX=wllvm++

cd src

extract-bc nano
llvm-dis-14 nano.bc

echo "@@@ LLVM IR File present in $(pwd)"