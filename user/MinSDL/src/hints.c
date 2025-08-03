#include "SDL_hints.h"
#include "stub.h"
SDL_bool SDL_SetHint(const char *name, const char *value) {
    stub("%s, %s", name, value);
    return SDL_FALSE;
}
