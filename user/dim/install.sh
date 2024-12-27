#!/bin/bash

set -e

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
