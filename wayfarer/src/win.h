// A one-shot, terminal "WIN" screen - world.c calls win_play() once,
// the moment both win conditions are met (the enemy defeated and the
// player in room (0,1)), and nothing ever un-hides it. See win.c.

#ifndef WAYFARER_WIN_H
#define WAYFARER_WIN_H

void win_play(void);

#endif
