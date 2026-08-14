// A "GAME OVER" screen - main.c calls gameover_play() once, the moment
// lives.c reports the last life spent. A Start press restarts the
// whole game, same as win.c's own win screen (main.c owns the shared
// restart handling for both terminal screens). See gameover.c.

#ifndef ASCENT_GAMEOVER_H
#define ASCENT_GAMEOVER_H

void gameover_play(void);

#endif
