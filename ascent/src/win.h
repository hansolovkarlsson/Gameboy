// A one-shot, terminal "WIN" screen - main.c calls win_play() once,
// the moment goal.c reports the flag reached, and nothing ever
// un-hides it (no restart yet - see docs/GAMEBOY_ROADMAP.md's own
// Milestone 4 entry). See win.c.

#ifndef ASCENT_WIN_H
#define ASCENT_WIN_H

void win_play(void);

#endif
