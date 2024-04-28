#if __APPLE__
    #define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700

#include "signals.h"
#include <errno.h>
#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include "error.h"

bool running = true;

static void stop_running(int signal) {
    ASSERT(signal == SIGINT);
    (void) signal;

    running = false;
}

static void resize(int signal) {
    ASSERT(signal == SIGWINCH);
    (void) signal;

    struct winsize win;
    ioctl(0, TIOCGWINSZ, &win);
    if (resizeterm(win.ws_row, win.ws_col) == ERR) ERROR("Unable to resize terminal window: %s.\n", strerror(errno));
}

void setup_signal_handlers(void) {
    struct sigaction action = {0};

    action.sa_handler = stop_running;
    sigaction(SIGINT, &action, NULL);

    action.sa_handler = resize;
    sigaction(SIGWINCH, &action, NULL);
}