#include "SDL_render.h"
#include "stub.h"
DECLSPEC int SDLCALL SDL_RenderClear(SDL_Renderer * renderer) {
    stub("%p", renderer);
    return -1;
}
DECLSPEC int SDLCALL SDL_UpdateTexture(SDL_Texture * texture, const SDL_Rect * rect, const void *pixels, int pitch) {
    stub("%p %p %p %d", texture, rect, pixels, pitch);
    return -1;
}

DECLSPEC int SDLCALL SDL_RenderCopy(SDL_Renderer * renderer,
                                    SDL_Texture * texture,
                                    const SDL_Rect * srcrect,
                                    const SDL_Rect * dstrect) {
    stub("%p %p %p %p", renderer, texture, srcrect, dstrect);
    return -1;
}

DECLSPEC void SDLCALL SDL_RenderPresent(SDL_Renderer * renderer) {
    stub("%p", renderer);
}

DECLSPEC void SDLCALL SDL_DestroyTexture(SDL_Texture * texture) {
    stub("%p", texture);
}

DECLSPEC int SDLCALL SDL_RenderSetLogicalSize(SDL_Renderer * renderer, int w, int h) {
    stub("%p %d %d", renderer, w, h);
    return -1;
}

DECLSPEC SDL_Texture * SDLCALL SDL_CreateTexture(SDL_Renderer * renderer,
                                                 Uint32 format,
                                                 int access, int w,
                                                 int h) {
    stub("%p %u %d %d %d", renderer, format, access, w, h);
    return NULL;
}
int SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode) {
    stub("%p %u", texture, blendMode);
    return -1;
}

DECLSPEC int SDLCALL SDL_SetRenderDrawColor(SDL_Renderer * renderer,
                                           Uint8 r, Uint8 g, Uint8 b,
                                           Uint8 a) {
    stub("%p %u %u %u %u", renderer, r, g, b, a);
    return 0;
}

DECLSPEC SDL_Renderer* SDLCALL SDL_CreateRenderer(SDL_Window * window, int index, Uint32 flags) {
    stub("%p %d %u", window, index, flags);
    return NULL;
}
