#!/bin/bash

set -e

if ! command -v x86_64-minos-gcc &> /dev/null; then
    echo "In order to build lua for MinOS you need the MinOS toolchain"
    echo "For more information checkout <path to MinOS>/user/toolchain/build.sh"
    echo "Make sure to add <path to MinOS>/user/toolchain/bin/binutils/bin/ to your PATH!"
fi 

lua_version="5.4.7"
if [ ! -f "lua-$lua_version.tar.gz" ]; then
    wget https://www.lua.org/ftp/lua-$(lua_version).tar.gz
fi
if [ ! -d "lua-$lua_version" ]; then
    tar -xzf lua-$(lua_version).tar.gz
fi
cd lua-$lua_version

if [ ! -f "patched" ]; then 
    patch -ruN -p1 < ../patch.patch
    touch patched
fi
make generic
cp -v src/lua src/luac ../../../initrd/user/ 
