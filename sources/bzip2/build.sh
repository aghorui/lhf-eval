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

SRC_PATH="bzip2-1.0.8"

cd "$SRC_PATH"

if [ -e 'Makefile' ]; then
    make clean
fi

make -j4 CC=wllvm CXX=wllvm++ CFLAGS="-O0 -g -D_FILE_OFFSET_BITS=64"

CC=wllvm CXX=wllvm++ extract-bc bzip2
CC=wllvm CXX=wllvm++ llvm-dis-14 bzip2.bc

echo "@@@ LLVM IR File present in $(pwd)"