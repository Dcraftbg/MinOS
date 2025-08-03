#include "SDL_timer.h"
#include "stub.h"

DECLSPEC void SDLCALL SDL_Delay(Uint32 ms) {
    stub("%u", ms);
}
Uint64 SDL_GetPerformanceFrequency(void) {
    stub("");
    return 0;
}
Uint64 SDL_GetPerformanceCounter(void) {
    stub("");
    return 0;
}
