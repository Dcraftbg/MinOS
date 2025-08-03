#include "SDL_audio.h"
#include "stub.h"
DECLSPEC void SDLCALL SDL_LockAudioDevice(SDL_AudioDeviceID dev) {
    stub("%u", dev);
}

DECLSPEC void SDLCALL SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) {
    stub("%u", dev);
}

DECLSPEC void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {
    stub("%u, %d", dev, pause_on);
}

DECLSPEC SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char *device, int iscapture, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int allowed_changes) {
    stub("%s %d %p %p %d", device, iscapture, desired, obtained, allowed_changes);
    return -1;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev) {
    stub("%u", dev);
}
