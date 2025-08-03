#include "SDL_video.h"
#include "stub.h"

void SDL_GetWindowSize(SDL_Window * window, int *w, int *h) {
    stub("%p %p %p", window, w, h);
}

void SDL_SetWindowSize(SDL_Window * window, int w, int h) {
    stub("%p %d %d", window, w, h);
}
void SDL_SetWindowTitle(SDL_Window *window, const char *title) {
    stub("%p %s", window, title);
}
void SDL_StartTextInput(void) {
    stub("");
}
SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags) {
    stub("%s %d %d %d %d %u", title, x, y, w, h, flags);
    return NULL;
}
int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags) {
    stub("%p %u", window, flags);
    return -1;
}
void SDL_SetWindowBordered(SDL_Window *window, SDL_bool bordered) {
    stub("%p %s", window, bordered ? "true" : "false");
}
