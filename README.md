# MinOS

MinOS is a mini operating system written in C.
(Minos is also the king of Crete in the greek mythology)

## Special Thanks 
- [@jakeSteinburger](https://github.com/jakeSteinburger) (u/JakeStBu) - testing and supporting the project!
- [@Tsoding](https://github.com/tsoding) - for the amazing [nob.h library](https://github.com/tsoding/musializer)
- [@lordmilko](https://github.com/lordmilko) - for the [precompiled binaries](https://github.com/lordmilko/i686-elf-tools) for gcc and binutils
- [nothings stb libraries](https://github.com/nothings/stb)

## Quickstart

The project uses a similar structure as that of [Cost](https://github.com/Dcraftbg/Cost/tree/main).
The basic idea for the build system is that it's "self-contained". No Makefiles, no special external tools.

Just a single C compiler is all you need.

To bootstrap the build system simply compile bootstrap.c and run it:
```sh
gcc bootstrap.c -o bootstrap
./bootstrap
```

**NOTE:** On Linux and Windows by default bootstrap will try and download a pre-compiled version of gcc for cross compiling.
If you're on MacOS or Linux or just want to use a pre-existing cross compiler, pass the `-GCC=<path to gcc>` and `-LD=<path to ld>` flags (if it's in your PATH, just gcc and ld should work fine).
If you've already generated the config.h file you'll also need to add `-fconfig` to force it to regenerate it or just edit the config.h directly.

Bootstrap will then proceed to build the build system. 

Now that our build system is bootstrapped we can just call it directly (any future updates to the build system would force it to rebuild itself):
```sh
./build
```

The build system has a plethora of different commands. To build and generate a bootable ISO, you can use the "build" command:
```sh
./build build
```

For quick iteration I also recommend using `bruh` which will both build the ISO and run it using qemu.

## Purpose

The MinOS is made to show my experience with Osdev so far and
to combine a lot of the things I like in an OS. 
As the name suggests the OS is "minified" into something minimalistic
and simple. The project is also meant to be used as a teaching basis from which 
other people can learn from as I try to display different aspects of osdev in
a more or less simple way (at least currently)

MinOS uses a lot of concepts that are based on Unix systems, but strives to create something which aims to provide speed and efficiency while avoiding some issues that many Unix systems have.

The OS is **not** Unix compatible (and doesn't have plans to be), even if it uses similar ideas.

## Progress & Plans

Progress is tracked over on trello where you can also find any plans for the future:
- https://trello.com/b/zdokafFr/minos

## Contributing

I'm open to contributions!

I love when the community helps in making something good, but I have to point out a few things.

Please don't make contributions that:
- Make something intentionally more obscure
- Add a huge dependency without much reasoning
- Implement or change an enormous part of the code and its structure (unless justified)

