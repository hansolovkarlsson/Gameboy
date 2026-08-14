// See score.h. Digit glyphs reused verbatim from
// prism/src/hud.c's own hud_digit_tiles (the classic 7-segment-display
// convention - public domain, not copied pixel art - already proven
// out and verified by rendering there) - same "already-proven
// hand-authored art, don't redraw it" reasoning player.c and win.c
// already give for reusing wayfarer/'s own sprite/letter art.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "score.h"

#define SCORE_DIGIT_TILE_BASE 13 // stage.c owns BG 0-3, win.c owns 4-12
#define SCORE_DIGIT_COUNT 10

static const uint8_t score_digit_tiles[SCORE_DIGIT_COUNT * 16] = {
    // 0
    0x7E, 0x7E, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3,
    0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x7E, 0x7E,
    // 1
    0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00,
    // 2
    0x7E, 0x7E, 0x7F, 0x7F, 0x03, 0x03, 0x7F, 0x7F,
    0xFE, 0xFE, 0xC0, 0xC0, 0xFE, 0xFE, 0x7E, 0x7E,
    // 3
    0x7E, 0x7E, 0x7F, 0x7F, 0x03, 0x03, 0x7F, 0x7F,
    0x7F, 0x7F, 0x03, 0x03, 0x7F, 0x7F, 0x7E, 0x7E,
    // 4
    0x00, 0x00, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF,
    0x7F, 0x7F, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00,
    // 5
    0x7E, 0x7E, 0xFE, 0xFE, 0xC0, 0xC0, 0xFE, 0xFE,
    0x7F, 0x7F, 0x03, 0x03, 0x7F, 0x7F, 0x7E, 0x7E,
    // 6
    0x7E, 0x7E, 0xFE, 0xFE, 0xC0, 0xC0, 0xFE, 0xFE,
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x7E, 0x7E,
    // 7
    0x7E, 0x7E, 0x7F, 0x7F, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00,
    // 8
    0x7E, 0x7E, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF,
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x7E, 0x7E,
    // 9
    0x7E, 0x7E, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF,
    0x7F, 0x7F, 0x03, 0x03, 0x7F, 0x7F, 0x7E, 0x7E,
};

// One CGB BG palette: color 0 the same dark navy as stage.c's own
// STAGE_PALETTE color 0, so the digit tiles' own unlit background
// blends into the open-air backdrop above tier 3 rather than reading
// as a separate box; color 3 a bright pale gold, legible against it
// (colors 1/2 unused - every digit byte pair here is low==high, the
// same "one solid color per lit row" convention every hand-drawn tile
// in this project relies on, so only indices 0 and 3 ever appear).
#define SCORE_PALETTE 2 // stage.c owns 0, win.c owns 1
static const palette_color_t score_palette[4] = {
    RGB(2, 2, 10), RGB(2, 2, 10), RGB(2, 2, 10), RGB(31, 29, 16),
};

#define SCORE_X 1
#define SCORE_Y 0
#define SCORE_DIGITS 5

static uint16_t score;

static void draw_score(void) {
    uint8_t tiles[SCORE_DIGITS];
    uint16_t v = score;
    for (int8_t i = SCORE_DIGITS - 1; i >= 0; i--) {
        tiles[i] = (uint8_t)(SCORE_DIGIT_TILE_BASE + (v % 10));
        v /= 10;
    }
    set_bkg_tiles(SCORE_X, SCORE_Y, SCORE_DIGITS, 1, tiles);

    // Explicit attribute stamp, not left to whatever the map already
    // has - the same lesson main.c's own restart bug (see
    // docs/GAMEBOY_ROADMAP.md's Milestone 5 entry) already taught for
    // stage_init(): win_play() can leave row 0's own attributes
    // pointing at WIN_PALETTE, and only a real restart ever exercises
    // that path a second time.
    uint8_t attrs[SCORE_DIGITS];
    for (uint8_t i = 0; i < SCORE_DIGITS; i++) attrs[i] = SCORE_PALETTE;
    set_bkg_attributes(SCORE_X, SCORE_Y, SCORE_DIGITS, 1, attrs);
}

void score_init(void) {
    set_bkg_palette(SCORE_PALETTE, 1, score_palette);
    set_bkg_data(SCORE_DIGIT_TILE_BASE, SCORE_DIGIT_COUNT, score_digit_tiles);
    score = 0;
    draw_score();
}

void score_add(uint16_t points) {
    score = (uint16_t)(score + points);
    draw_score();
}
