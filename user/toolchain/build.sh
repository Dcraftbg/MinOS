#!/bin/bash
# NOTE: A big portion of this is taken/inspired from
# https://github.com/Bananymous/banan-os/blob/main/toolchain/build.sh

set -e
BINUTILS_VERSION="binutils-2.39"
BINUTILS_TAR="$BINUTILS_VERSION.tar.gz"
BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/$BINUTILS_TAR"
BINUTILS_DIR="bin/binutils"


GCC_VERSION="gcc-12.2.0"
GCC_TAR="$GCC_VERSION.tar.gz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/$GCC_VERSION/$GCC_TAR"
GCC_DIR="bin/gcc"

SYSROOT="bin/sysroot"

mkdir -p bin/
mkdir -p $BINUTILS_DIR 
mkdir -p $GCC_DIR 
mkdir -p $SYSROOT


BINUTILS_DIRABS=$(realpath $BINUTILS_DIR)
GCC_DIRABS=$(realpath $GCC_DIR)
SYSROOTABS=$(realpath $SYSROOT)
MINOSROOT=../../

setup_sysroot () {
    mkdir -p $SYSROOT/usr
    cp -r $MINOSROOT/user/libc/include $MINOSROOT/libs/std/include $SYSROOT/usr
    mkdir -p $SYSROOT/usr/lib
    cp $MINOSROOT/bin/user/crt/start.o $SYSROOT/usr/lib/crt0.o
    ar -cr $SYSROOT/usr/lib/libc.a $MINOSROOT/bin/user/libc/*.o $MINOSROOT/bin/user/libc/sys/*.o $MINOSROOT/bin/std/*.o
}
build_binutils () {
    echo "Building ${BINUTILS_VERSION}"
    cd $BINUTILS_DIR
    if [ ! -d $BINUTILS_VERSION ]; then
        if [ ! -f $BINUTILS_TAR ]; then
            wget $BINUTILS_URL
        fi
        tar -xf $BINUTILS_TAR
        patch -ruN -p1 -d $BINUTILS_VERSION < ../../binutils-2.39.patch
    fi
    cd $BINUTILS_VERSION
    ./configure \
        --target="x86_64-minos" \
        --disable-shared \
        --prefix="$BINUTILS_DIRABS" \
        --with-sysroot="$SYSROOTABS" \
        --enable-initfini-array \
        --enable-lto \
        --disable-nls \
        --disable-werror

    make -j12
    make install
} 
build_gcc () {
    echo "Building ${GCC_VERSION}"
    cd $GCC_DIRABS
    if [ ! -d $GCC_VERSION ]; then
        if [ ! -f $GCC_TAR ]; then
            wget $GCC_URL
        fi
        tar -xf $GCC_TAR
        patch -ruN -p1 -d $GCC_VERSION < ../../gcc-12.2.0.patch
    fi
    cd $GCC_VERSION
    mkdir -p build
    cd build
    ../configure \
        --target="x86_64-minos" \
        --disable-shared \
        --prefix="$BINUTILS_DIRABS" \
        --with-sysroot="$SYSROOTABS" \
        --enable-initfini-array \
        --enable-lto \
        --disable-nls \
        --enable-languages=c

    XCFLAGS="-mcmodel=large -mno-red-zone"
    make -j12 all-gcc
    make -j12 all-target-libgcc CFLAGS_FOR_TARGET="$XCFLAGS"
    make install-gcc
    make install-target-libgcc
}
help () {
    echo "./build.sh [subcmd...]"
    echo "Subcmds:"
    echo "  - sysroot  - updates sysroot"
    echo "  - binutils - builds binutils"
    echo "  - gcc      - builds gcc"
    echo "  - all      - Builds everything"
}
if [ "$#" -eq 0 ]; then
    echo "Missing subcommand!"
    exit 1
fi
for arg in "$@"; do
    case $arg in
        sysroot)
            setup_sysroot
            ;;
        binutils)
            build_binutils
            ;;
        gcc)
            build_gcc
            ;;
        all)
            setup_sysroot
            build_binutils
            build_gcc
            ;;
        *)
            echo "Invalid subcommand: $arg"
            help
            exit 1
    esac
done
