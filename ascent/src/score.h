// A live 5-digit score readout on background row 0 (open air above
// tier 3 in stage.c's own tile map - never touched by any girder or
// ladder tile, so nothing to dodge). See score.c.

#ifndef ASCENT_SCORE_H
#define ASCENT_SCORE_H

#include <stdint.h>

// Loads the digit tileset/palette and resets the score to 0, redrawing
// it immediately. Call once at startup and again on every restart
// (main.c's own won/Start handling), same as every other module's own
// _init().
void score_init(void);

// Adds points to the running total and redraws. Call only when points
// is nonzero - same "redraw only on real change" discipline
// prism/src/hud.h's own hud_set_score() already documents.
void score_add(uint16_t points);

#endif
