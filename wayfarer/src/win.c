// See win.h. A small, self-contained text overlay - the 3 letters
// needed for "WIN" (W, I, N) plus the letters needed for a "PRESS
// START" restart hint (P, R, E, S, T, A - reusing the exact same
// bytes title.c's own P/R/E/S/T/A already use, since it's the same
// hand-authored 5x7 block-letter, left-aligned-for-consistent-spacing
// convention the sibling prism/ project's own title.c and this
// project's own title.c already established, public domain block-font
// shapes, not copied art) - still not a general alphabet, just every
// letter these two words need. Reuses room.h's own FLOOR_TILE_ID
// directly both to blank the screen under a new palette (the same
// "one tile, palette swap" trick heart_hud.c's full/empty hearts
// already use) and, since the screen is already that color underneath,
// as the space character in "PRESS START" - no second blank-tile
// bitmap needed either way.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "room.h"
#include "win.h"

#define WIN_TILE_BASE 3 // room.c owns BG tile IDs 0-2 (floor/wall/door)
#define WIN_W (WIN_TILE_BASE + 0)
#define WIN_I (WIN_TILE_BASE + 1)
#define WIN_N (WIN_TILE_BASE + 2)
#define WIN_P (WIN_TILE_BASE + 3)
#define WIN_R (WIN_TILE_BASE + 4)
#define WIN_E (WIN_TILE_BASE + 5)
#define WIN_S (WIN_TILE_BASE + 6)
#define WIN_T (WIN_TILE_BASE + 7)
#define WIN_A (WIN_TILE_BASE + 8)
#define WIN_TILE_COUNT 9

static const uint8_t win_tiles[WIN_TILE_COUNT * 16] = {
    // W
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0xA8, 0xA8,
    0xA8, 0xA8, 0xD8, 0xD8, 0x88, 0x88, 0x00, 0x00,
    // I
    0xF8, 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0xF8, 0xF8, 0x00, 0x00,
    // N
    0x88, 0x88, 0xC8, 0xC8, 0xA8, 0xA8, 0xA8, 0xA8,
    0x98, 0x98, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00,
    // P
    0xF0, 0xF0, 0x88, 0x88, 0x88, 0x88, 0xF0, 0xF0,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00,
    // R
    0xF0, 0xF0, 0x88, 0x88, 0x88, 0x88, 0xF0, 0xF0,
    0xA0, 0xA0, 0x90, 0x90, 0x88, 0x88, 0x00, 0x00,
    // E
    0xF8, 0xF8, 0x80, 0x80, 0x80, 0x80, 0xF0, 0xF0,
    0x80, 0x80, 0x80, 0x80, 0xF8, 0xF8, 0x00, 0x00,
    // S
    0x78, 0x78, 0x80, 0x80, 0x80, 0x80, 0x70, 0x70,
    0x08, 0x08, 0x08, 0x08, 0xF0, 0xF0, 0x00, 0x00,
    // T
    0xF8, 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00,
    // A
    0x70, 0x70, 0x88, 0x88, 0x88, 0x88, 0xF8, 0xF8,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00,
};

// BG palette index 2 - room.c owns 0 (room) and 1 (door). A bright
// celebratory gold/white, distinct from both.
#define WIN_PALETTE 2
static const palette_color_t win_palette[4] = {
    RGB(20, 16, 2), RGB(26, 21, 4), RGB(31, 27, 10), RGB(31, 31, 24),
};

#define SCREEN_TILE_W 20
#define SCREEN_TILE_H 18

static uint8_t glyph_tile(char c) {
    switch (c) {
        case 'P': return WIN_P;
        case 'R': return WIN_R;
        case 'E': return WIN_E;
        case 'S': return WIN_S;
        case 'T': return WIN_T;
        case 'A': return WIN_A;
        default: return FLOOR_TILE_ID; // space - already the fill color
    }
}

// Draws a short (<=20 char) string on one background row, horizontally
// centered, in WIN_PALETTE - same shape as title.c's own draw_centered.
static void draw_centered(const char *text, uint8_t len, uint8_t y) {
    uint8_t tiles[20];
    for (uint8_t i = 0; i < len; i++) tiles[i] = glyph_tile(text[i]);
    uint8_t x = (uint8_t)((SCREEN_TILE_W - len) / 2);
    set_bkg_tiles(x, y, len, 1, tiles);

    uint8_t attrs[20];
    for (uint8_t i = 0; i < len; i++) attrs[i] = WIN_PALETTE;
    set_bkg_attributes(x, y, len, 1, attrs);
}

void win_play(void) {
    // Real-hardware-safe LCD-disable timing (only during VBlank) -
    // same pattern every bulk redraw in this project already uses -
    // so the blank+letters below never land on a live, actively-
    // rendering display.
    wait_vbl_done();
    DISPLAY_OFF;

    HIDE_SPRITES; // turns off the whole OBJ layer at once - simpler and
                   // more robust than win.c calling into every other
                   // module's own sprite-hide function individually

    set_bkg_palette(WIN_PALETTE, 1, win_palette);
    set_bkg_data(WIN_TILE_BASE, WIN_TILE_COUNT, win_tiles);

    uint8_t blank_row[SCREEN_TILE_W];
    uint8_t blank_attrs[SCREEN_TILE_W];
    for (uint8_t i = 0; i < SCREEN_TILE_W; i++) {
        blank_row[i] = FLOOR_TILE_ID;
        blank_attrs[i] = WIN_PALETTE;
    }
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        set_bkg_tiles(0, y, SCREEN_TILE_W, 1, blank_row);
        set_bkg_attributes(0, y, SCREEN_TILE_W, 1, blank_attrs);
    }

    uint8_t win_word[3] = { WIN_W, WIN_I, WIN_N };
    uint8_t word_attrs[3] = { WIN_PALETTE, WIN_PALETTE, WIN_PALETTE };
    uint8_t x = (uint8_t)((SCREEN_TILE_W - 3) / 2);
    uint8_t y = 8;
    set_bkg_tiles(x, y, 3, 1, win_word);
    set_bkg_attributes(x, y, 3, 1, word_attrs);

    draw_centered("PRESS START", 11, 11);

    SHOW_BKG;
    DISPLAY_ON;
}
