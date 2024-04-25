#define _XOPEN_SOURCE 700
#include <errno.h>
#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include "signals.h"
#include "utils/error.h"

bool running = true;

static void stop_running(int signal) {
    ASSERT(signal == SIGINT);
    running = false;
}

static void resize(int signal) {
    ASSERT(signal == SIGWINCH);

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