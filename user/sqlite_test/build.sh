#!/bin/bash
# TODO: nob.c and a better build system
CC=x86_64-minos-gcc
set -xe
YEAR=2024
VERSION="3470200"
SQLITE_ZIP="sqlite-amalgamation-$VERSION.zip"
if [ ! -f $SQLITE_ZIP ]; then
    wget https://www.sqlite.org/$YEAR/$SQLITE_ZIP
fi
if [ ! -f sqlite3.c ]; then
    unzip $SQLITE_ZIP
    mv "sqlite-amalgamation-$VERSION/sqlite*" .
fi
if ! patch -R -p0 -s -f --dry-run < sqlite3.c.patch; then
    patch -p0 < sqlite3.c.patch
fi
if [ "sqlite3_minos.c" -nt "sqlite3.o" ]; then
    $CC sqlite3_minos.c -c -g -o sqlite3.o
fi
$CC sqlite3_basic.c -o sqlite3_basic sqlite3.o
cp -v sqlite3_basic ../../../initrd/user/
