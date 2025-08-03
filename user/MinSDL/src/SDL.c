#include "SDL.h"
#include "stub.h"

int SDL_InitSubSystem(Uint32 flags) {
    flags &= ~(SDL_INIT_TIMER | SDL_INIT_EVENTS);
    if(flags) {
        stub("flags=%08X", flags);
    }
    return 0;
}
int SDL_Init(Uint32 flags) {
    return SDL_InitSubSystem(flags);
}
void SDL_Quit(void) {
    stub("");
}
