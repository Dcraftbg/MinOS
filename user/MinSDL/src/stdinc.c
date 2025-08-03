#include "SDL_stdinc.h"
#include <string.h>
DECLSPEC void *SDLCALL SDL_memset(SDL_OUT_BYTECAP(len) void *dst, int c, size_t len) {
    return memset(dst, c, len);
}
