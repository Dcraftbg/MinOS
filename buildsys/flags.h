#pragma once
// TODO: Consider moving to ../config.h
#define CFLAGS\
    "-g",\
    "-nostdlib",\
    "-march=x86-64",\
    "-ffreestanding",\
    "-static", \
    "-Werror", "-Wno-unused-function",\
    "-Wall", \
    "-fno-stack-protector", \
    "-fcf-protection=none", \
    "-O2",\
    /*"-fomit-frame-pointer", "-fno-builtin", */\
    /*"-mno-red-zone",*/\
    "-mno-mmx",\
    "-mno-sse", "-mno-sse2",\
    "-mno-3dnow",\
    "-fPIC",\
    "-I", "libs/std/include",\
    "-I", "kernel/src"

#define USER_CFLAGS\
    "-g",\
    "-nostdlib",\
    "-march=x86-64",\
    "-ffreestanding",\
    "-static", \
    "-Werror", "-Wno-unused-function",\
    "-Wall", \
    "-fno-stack-protector", \
    "-mno-mmx",\
    /*"-mno-sse", "-mno-sse2",*/\
    "-mno-3dnow",\
    "-fPIC",\
    "-I", "libs/std/include",
#define LDFLAGS "-g"
