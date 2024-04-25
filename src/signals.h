#ifndef SIGNALS_H
#define SIGNALS_H

#include <ncurses.h>

extern bool running;

void setup_signal_handlers(void);

#endif  // SIGNALS_H