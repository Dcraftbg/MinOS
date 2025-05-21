#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#ifndef STUI_REALLOC
# include <stdlib.h>
# define STUI_REALLOC realloc
#endif

// CORE:
void stui_setsize(size_t x, size_t y);
void stui_putchar(size_t x, size_t y, int c);
void stui_refresh(void);
// Raw API
void stui_goto(size_t x, size_t y);
void stui_clear(void);
// Terminal API
// stui Terminal interface
// Character echo
#define STUI_TERM_FLAG_ECHO    (1 << 0)
// Instant or non-canonical mode
#define STUI_TERM_FLAG_INSTANT (1 << 1)
typedef uint8_t stui_term_flag_t;


void stui_term_get_size(size_t *w, size_t *h);
stui_term_flag_t stui_term_get_flags(void);
void stui_term_set_flags(stui_term_flag_t flags);
// Helper functions
static void stui_term_enable_echo(void) {
    stui_term_set_flags(stui_term_get_flags() | STUI_TERM_FLAG_ECHO);
}
static void stui_term_disable_echo(void) {
    stui_term_set_flags(stui_term_get_flags() & ~STUI_TERM_FLAG_ECHO);
}
static void stui_term_enable_instant(void) {
    stui_term_set_flags(stui_term_get_flags() | STUI_TERM_FLAG_INSTANT);
}
static void stui_term_disable_instant(void) {
    stui_term_set_flags(stui_term_get_flags() & ~STUI_TERM_FLAG_INSTANT);
}

#ifdef STUI_IMPLEMENTATION
#define STUI_BUFFER_COUNT 2
static uint8_t _stui_back_buffer = 0; 
static char* _stui_buffers[STUI_BUFFER_COUNT] = { 0 };
static size_t _stui_width = 0, _stui_height = 0;
void stui_setsize(size_t x, size_t y) {
    _stui_width = x;
    _stui_height = y;
    size_t n = x*y*sizeof(_stui_buffers[0]);
    for(size_t i = 0; i < STUI_BUFFER_COUNT; ++i) {
        _stui_buffers[i] = STUI_REALLOC(_stui_buffers[i], n);
        assert(_stui_buffers[i]);
        memset(_stui_buffers[i], ' ', n);
    }
}
void stui_putchar(size_t x, size_t y, int c) {
    char* buffer = _stui_buffers[_stui_back_buffer];
    assert(x < _stui_width);
    assert(y < _stui_height);
    buffer[y * _stui_width + x] = c;
}
void stui_refresh(void) {
    char* back  = _stui_buffers[_stui_back_buffer];
    char* front = _stui_buffers[(_stui_back_buffer + 1) % STUI_BUFFER_COUNT];
    for(size_t i = 0; i < _stui_width*_stui_height; ++i) {
        if(back[i] != front[i]) {
            size_t x = i % _stui_width, y = i / _stui_width;
            stui_goto(x, y);
            printf("%c", back[i]);
            fflush(stdout);
            front[i] = back[i];
        }
    }
    _stui_back_buffer = (_stui_back_buffer + 1) % STUI_BUFFER_COUNT;
}

// Raw API
void stui_goto(size_t x, size_t y) {
    printf("\033[%zu;%zuH", y+1, x+1);
    fflush(stdout);
}
void stui_clear(void) {
    printf("\033[2J");
    printf("\033[H");
    fflush(stdout);
}

// Utilities
void stui_window_border(size_t x, size_t y, size_t w, size_t h, int tb, int lr, int corner) {
    for(size_t dx = x + 1; dx < (x + w); dx++) {
        stui_putchar(dx, y  , tb);
        stui_putchar(dx, y+h, tb);
    }
    for(size_t dy = y + 1; dy < (y + h); dy++) {
        stui_putchar(x    , dy, lr);
        stui_putchar(x + w, dy, lr);
    }
    stui_putchar(x    , y    , corner);
    stui_putchar(x + w, y    , corner);
    stui_putchar(x    , y + h, corner);
    stui_putchar(x + w, y + h, corner);
}

// Terminal API
#ifdef _MINOS
# include <minos/tty/tty.h>
#else
# include <termios.h>
# include <unistd.h>
# include <signal.h>
# include <sys/ioctl.h>
#endif

void stui_term_get_size(size_t *w, size_t *h) {
#ifdef _MINOS
    TtySize size = { 0 };
    if(tty_get_size(0, &size) < 0) {
        *w = 80;
        *h = 24;
        return;
    }
    *w = size.width;
    *h = size.height;
#else
    struct winsize winsz;
    ioctl(0, TIOCGWINSZ, &winsz);
    *w = winsz.ws_col;
    *h = winsz.ws_row;
#endif
}
stui_term_flag_t stui_term_get_flags(void) {
    stui_term_flag_t flags = 0;
#ifdef _MINOS
    ttyflags_t tty_flags;
    tty_get_flags(fileno(stdin), &tty_flags);
    if(tty_flags & TTY_ECHO)    flags |= STUI_TERM_FLAG_ECHO;
    if(tty_flags & TTY_INSTANT) flags |= STUI_TERM_FLAG_INSTANT;
#else
    // Assume Unix platform
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    if(term.c_lflag & ECHO)      flags |= STUI_TERM_FLAG_ECHO;
    if(!(term.c_lflag & ICANON)) flags |= STUI_TERM_FLAG_INSTANT;
#endif
    return flags;
}
void stui_term_set_flags(stui_term_flag_t flags) {
#ifdef _MINOS
    ttyflags_t tty_flags;
    tty_get_flags(fileno(stdin), &tty_flags);
    tty_flags &= ~(TTY_ECHO | TTY_INSTANT);
    if(flags & STUI_TERM_FLAG_ECHO) tty_flags |= TTY_ECHO;
    if(flags & STUI_TERM_FLAG_INSTANT) tty_flags |= TTY_INSTANT;
    tty_set_flags(fileno(stdin), tty_flags);
#else
    // Assume Unix platform
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ECHO | ICANON);
    if(flags & STUI_TERM_FLAG_ECHO) term.c_lflag |= ECHO;
    if(!(flags & STUI_TERM_FLAG_INSTANT)) term.c_lflag |= ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
#endif
}
#endif
