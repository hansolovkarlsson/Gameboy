// A live single-digit lives counter, the right half of the same
// background-row-0 HUD score.c's own counter occupies the left half
// of. See lives.c.

#ifndef ASCENT_LIVES_H
#define ASCENT_LIVES_H

#include <stdint.h>

// Resets lives to the starting count and redraws. Must run after
// score_init() - it reuses score.c's own already-loaded digit tiles/
// palette rather than loading a second copy (see score.h). Call once
// at startup and again on every restart, same as every other module's
// own _init().
void lives_init(void);

// Spends one life and redraws. Returns 1 if that was the last one (the
// run is over - main.c should show the game-over screen instead of
// respawning), 0 if lives remain (main.c should respawn as usual).
uint8_t lives_lose(void);

#endif
