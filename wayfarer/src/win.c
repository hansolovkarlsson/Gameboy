// See win.h. A small, self-contained text overlay - only the 3
// letters actually needed for "WIN" (W, I, N), the same hand-authored
// 5x7 block-letter, left-aligned-for-consistent-spacing convention the
// sibling prism/ project's own title.c already established (public
// domain block-font shapes, not copied art), not a general alphabet.
// Reuses room.h's own FLOOR_TILE_ID directly to blank the screen under
// a new palette - the same "one tile, palette swap" trick heart_hud.c's
// full/empty hearts already use, so no second blank-tile bitmap is
// needed.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "room.h"
#include "win.h"

#define WIN_TILE_BASE 3 // room.c owns BG tile IDs 0-2 (floor/wall/door)
#define WIN_W (WIN_TILE_BASE + 0)
#define WIN_I (WIN_TILE_BASE + 1)
#define WIN_N (WIN_TILE_BASE + 2)
#define WIN_TILE_COUNT 3

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
};

// BG palette index 2 - room.c owns 0 (room) and 1 (door). A bright
// celebratory gold/white, distinct from both.
#define WIN_PALETTE 2
static const palette_color_t win_palette[4] = {
    RGB(20, 16, 2), RGB(26, 21, 4), RGB(31, 27, 10), RGB(31, 31, 24),
};

#define SCREEN_TILE_W 20
#define SCREEN_TILE_H 18

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

    SHOW_BKG;
    DISPLAY_ON;
}
