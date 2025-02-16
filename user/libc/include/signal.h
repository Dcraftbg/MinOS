#pragma once
typedef void (*sighandler_t)(int);
#define SIG_IGN NULL /*TODO: _sig_ign*/
#define SIG_DFL NULL
// TODO: Actually implement this
sighandler_t signal(int signum, sighandler_t handler);
