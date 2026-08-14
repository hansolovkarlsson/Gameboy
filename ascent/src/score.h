// A live 5-digit score readout on background row 0 (open air above
// tier 3 in stage.c's own tile map - never touched by any girder or
// ladder tile, so nothing to dodge). See score.c.

#ifndef ASCENT_SCORE_H
#define ASCENT_SCORE_H

#include <stdint.h>

// Exported so lives.c can draw its own single-digit readout using the
// exact same already-loaded VRAM tiles/palette rather than loading a
// second identical copy - the two fields render simultaneously as one
// persistent HUD row, so sharing the live tile IDs (not just the byte
// values) is the natural choice here, unlike win.c's own W/I/N reuse
// or score.c's own digit reuse from prism/, both of which copy bytes
// across projects/contexts that are never resident in VRAM at the same
// time. lives_init() must run after score_init() - see lives.h.
#define SCORE_DIGIT_TILE_BASE 13 // stage.c owns BG 0-3, win.c owns 4-12
#define SCORE_PALETTE 2 // stage.c owns 0, win.c owns 1

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
