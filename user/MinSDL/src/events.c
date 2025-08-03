#include "SDL_events.h"
#include "stub.h"

DECLSPEC int SDLCALL SDL_PushEvent(SDL_Event * event) {
    stub("%p", event);
    return -1;
}
Uint32 SDL_RegisterEvents(int numevents) {
    stub("%d", numevents);
    return 0;
}
Uint8 SDL_EventState(Uint32 type, int state) {
    stub("%u %d", type ,state);
    return 0;
}
int SDL_PollEvent(SDL_Event *event) {
    stub("%p", event);
    return -1;
}

int SDL_PeepEvents(SDL_Event * events, int numevents, SDL_eventaction action,
                   Uint32 minType, Uint32 maxType) {
    stub("%p %d %d %u %u", events, numevents, action, minType, maxType);
    return -1;
}
int SDL_WaitEvent(SDL_Event *event) {
    stub("%p", event);
    return -1;
}
