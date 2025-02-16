#include <signal.h>
#include <stdio.h>
sighandler_t signal(int signum, sighandler_t handler) {
    fprintf(stderr, "<signal stub %d %p>\n", signum, handler);
    return NULL;
}
