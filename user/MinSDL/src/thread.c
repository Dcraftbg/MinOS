#include "SDL_thread.h"
#include "stub.h"
DECLSPEC SDL_Thread *SDLCALL SDL_CreateThread(SDL_ThreadFunction fn, const char *name, void *data) {
    stub("%p %s %p", fn, name, data);
    return NULL;
}
void SDL_DetachThread(SDL_Thread *thread) {
    stub("%p", thread);
}
