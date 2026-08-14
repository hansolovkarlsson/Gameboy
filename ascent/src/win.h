// A "WIN" screen - main.c calls win_play() once, the moment goal.c
// reports the flag reached. main.c itself owns the one way out: a
// Start press restarts the whole game (see main.c's own won/prev_joy
// handling), the same escape wayfarer/src/world.c already gives its
// own win screen. See win.c.

#ifndef ASCENT_WIN_H
#define ASCENT_WIN_H

void win_play(void);

#endif
