// See gameover.h. A small, self-contained text overlay - "GAME OVER"
// plus the same "PRESS START" restart hint win.c's own win screen
// uses. Six of the ten letters needed (A, E, P, R, S, T) are the exact
// same hand-authored 5x7 block-letter bytes win.c already has - copied
// verbatim here rather than shared at runtime, since win.c's own tile
// data is only ever resident in VRAM during an actual win, never
// during a game over (the two terminal screens are mutually exclusive
// within a single run, so each must be able to load its own copy
// independently). The remaining four (G, M, O, V) are newly authored
// in the exact same convention - a standard 5x7 dot-matrix block font,
// left-aligned within each 8x8 tile, public domain shapes not copied
// from anywhere - and verified by actually rendering them through this
// project's own emulator before being committed here, same discipline
// every hand-drawn tile in this project already relies on.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "stage.h"
#include "gameover.h"

#define GAMEOVER_TILE_BASE 23 // stage.c owns 0-3, win.c 4-12, score.c 13-22
#define GAMEOVER_A (GAMEOVER_TILE_BASE + 0)
#define GAMEOVER_E (GAMEOVER_TILE_BASE + 1)
#define GAMEOVER_G (GAMEOVER_TILE_BASE + 2)
#define GAMEOVER_M (GAMEOVER_TILE_BASE + 3)
#define GAMEOVER_O (GAMEOVER_TILE_BASE + 4)
#define GAMEOVER_P (GAMEOVER_TILE_BASE + 5)
#define GAMEOVER_R (GAMEOVER_TILE_BASE + 6)
#define GAMEOVER_S (GAMEOVER_TILE_BASE + 7)
#define GAMEOVER_T (GAMEOVER_TILE_BASE + 8)
#define GAMEOVER_V (GAMEOVER_TILE_BASE + 9)
#define GAMEOVER_TILE_COUNT 10

static const uint8_t gameover_tiles[GAMEOVER_TILE_COUNT * 16] = {
    // A (win.c's own bytes, verbatim)
    0x70, 0x70, 0x88, 0x88, 0x88, 0x88, 0xF8, 0xF8,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00,
    // E (win.c's own bytes, verbatim)
    0xF8, 0xF8, 0x80, 0x80, 0x80, 0x80, 0xF0, 0xF0,
    0x80, 0x80, 0x80, 0x80, 0xF8, 0xF8, 0x00, 0x00,
    // G
    0x70, 0x70, 0x88, 0x88, 0x80, 0x80, 0xB8, 0xB8,
    0x88, 0x88, 0x88, 0x88, 0x70, 0x70, 0x00, 0x00,
    // M
    0x88, 0x88, 0xD8, 0xD8, 0xA8, 0xA8, 0xA8, 0xA8,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00,
    // O
    0x70, 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x70, 0x70, 0x00, 0x00,
    // P (win.c's own bytes, verbatim)
    0xF0, 0xF0, 0x88, 0x88, 0x88, 0x88, 0xF0, 0xF0,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00,
    // R (win.c's own bytes, verbatim)
    0xF0, 0xF0, 0x88, 0x88, 0x88, 0x88, 0xF0, 0xF0,
    0xA0, 0xA0, 0x90, 0x90, 0x88, 0x88, 0x00, 0x00,
    // S (win.c's own bytes, verbatim)
    0x78, 0x78, 0x80, 0x80, 0x80, 0x80, 0x70, 0x70,
    0x08, 0x08, 0x08, 0x08, 0xF0, 0xF0, 0x00, 0x00,
    // T (win.c's own bytes, verbatim)
    0xF8, 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00,
    // V
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x50, 0x50, 0x20, 0x20, 0x00, 0x00,
};

// BG palette index 3 - stage.c owns 0, win.c owns 1, score.c owns 2. A
// somber dark red/gray, deliberately distinct from win.c's own bright
// celebratory gold - the two terminal screens should never read alike.
#define GAMEOVER_PALETTE 3
static const palette_color_t gameover_palette[4] = {
    RGB(6, 1, 1), RGB(12, 3, 3), RGB(20, 6, 6), RGB(28, 26, 24),
};

static uint8_t glyph_tile(char c) {
    switch (c) {
        case 'A': return GAMEOVER_A;
        case 'E': return GAMEOVER_E;
        case 'G': return GAMEOVER_G;
        case 'M': return GAMEOVER_M;
        case 'O': return GAMEOVER_O;
        case 'P': return GAMEOVER_P;
        case 'R': return GAMEOVER_R;
        case 'S': return GAMEOVER_S;
        case 'T': return GAMEOVER_T;
        case 'V': return GAMEOVER_V;
        default: return EMPTY_TILE; // space - already the fill color
    }
}

// Draws a short (<=20 char) string on one background row, horizontally
// centered, in GAMEOVER_PALETTE - same shape as win.c's own
// draw_centered().
static void draw_centered(const char *text, uint8_t len, uint8_t y) {
    uint8_t tiles[20];
    for (uint8_t i = 0; i < len; i++) tiles[i] = glyph_tile(text[i]);
    uint8_t x = (uint8_t)((STAGE_TILE_W - len) / 2);
    set_bkg_tiles(x, y, len, 1, tiles);

    uint8_t attrs[20];
    for (uint8_t i = 0; i < len; i++) attrs[i] = GAMEOVER_PALETTE;
    set_bkg_attributes(x, y, len, 1, attrs);
}

void gameover_play(void) {
    // Real-hardware-safe LCD-disable timing (only during VBlank) -
    // same pattern win.c and every bulk redraw in this project already
    // uses.
    wait_vbl_done();
    DISPLAY_OFF;

    HIDE_SPRITES; // the player, both barrel slots, and the goal flag
                   // all disappear together - same convention win.c's
                   // own win_play() already uses.

    set_bkg_palette(GAMEOVER_PALETTE, 1, gameover_palette);
    set_bkg_data(GAMEOVER_TILE_BASE, GAMEOVER_TILE_COUNT, gameover_tiles);

    uint8_t blank_row[STAGE_TILE_W];
    uint8_t blank_attrs[STAGE_TILE_W];
    for (uint8_t i = 0; i < STAGE_TILE_W; i++) {
        blank_row[i] = EMPTY_TILE;
        blank_attrs[i] = GAMEOVER_PALETTE;
    }
    for (uint8_t y = 0; y < STAGE_TILE_H; y++) {
        set_bkg_tiles(0, y, STAGE_TILE_W, 1, blank_row);
        set_bkg_attributes(0, y, STAGE_TILE_W, 1, blank_attrs);
    }

    draw_centered("GAME OVER", 9, 8);
    draw_centered("PRESS START", 11, 11);

    SHOW_BKG;
    DISPLAY_ON;
}
