#include "SDL_joystick.h"
#include "stub.h"
int SDL_NumJoysticks(void) {
    return 0;
}

SDL_Joystick *SDL_JoystickOpen(int device_index) {
    stub("%d", device_index);
    return NULL;
}
