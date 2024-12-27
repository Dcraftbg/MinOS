#!/bin/bash

set -e

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
