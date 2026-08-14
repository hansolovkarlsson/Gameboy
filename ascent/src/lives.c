// See lives.h. Draws a single digit at the opposite end of background
// row 0 from score.c's own counter - same two-field, one-row HUD shape
// prism/src/hud.c already established (its own SCORE_X=1/MOVES_X=17
// split). Reuses score.c's own already-loaded digit tiles/palette
// directly (score.h exports SCORE_DIGIT_TILE_BASE/SCORE_PALETTE) -
// both fields render simultaneously as one persistent HUD, so there's
// nothing to gain from a second, identical VRAM copy.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "lives.h"
#include "score.h"

// Real Donkey Kong (1981)'s own standard life count, not picked
// arbitrarily.
#define STARTING_LIVES 3

#define LIVES_X 18
#define LIVES_Y 0

static uint8_t lives;

static void draw_lives(void) {
    uint8_t tile = (uint8_t)(SCORE_DIGIT_TILE_BASE + lives);
    set_bkg_tiles(LIVES_X, LIVES_Y, 1, 1, &tile);

    // Explicit attribute stamp - the same reason score.c's own
    // draw_score() already gives (win.c/gameover.c can leave row 0's
    // attributes pointing elsewhere; only a real restart re-exercises
    // this path).
    uint8_t attr = SCORE_PALETTE;
    set_bkg_attributes(LIVES_X, LIVES_Y, 1, 1, &attr);
}

void lives_init(void) {
    lives = STARTING_LIVES;
    draw_lives();
}

uint8_t lives_lose(void) {
    if (lives > 0) lives--;
    draw_lives();
    return lives == 0;
}
