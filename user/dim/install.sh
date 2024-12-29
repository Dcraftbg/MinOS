#!/bin/bash

set -e

if ! command -v x86_64-minos-gcc &> /dev/null; then
    echo "In order to build dim for MinOS you need the MinOS toolchain"
    echo "For more information checkout <path to MinOS>/user/toolchain/build.sh"
    echo "Make sure to add <path to MinOS>/user/toolchain/bin/binutils/bin/ to your PATH!"
    exit 1
fi 

if [ ! -d "dim" ]; then
    git clone https://github.com/Dcraftbg/dim.git
fi
cd dim
if [ ! -f "patched" ]; then
    git apply ../patch.patch
    touch patched
fi
make
cp -v dim ../../../initrd/user/
