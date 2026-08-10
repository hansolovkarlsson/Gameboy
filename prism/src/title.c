// See title.h. A small custom letter tileset - only the 10 distinct
// letters actually needed across "PRISM", "PRESS START", and "HIGH"
// (P, R, I, S, M, E, T, A, H, G) plus a blank/space tile, 11
// single-8x8-tile glyphs - not GBDK's console/font system, same
// tile-ID-collision reasoning Milestone 5 already established for
// hud.c's digits. Each letterform is the standard, widely-known 5x7
// dot-matrix block-letter shape (the same public-domain convention as
// hud.c's 7-segment digits - not copied art), left-aligned within the
// 8-bit row for a consistent right-side letter-spacing column, computed
// via python3 -c "print(hex(int('11110',2)<<3))" style row-by-row
// shifts, then duplicated per row (both GB 2bpp bitplane bytes
// identical) for the same 2-color (0=background, 3=fill) scheme
// gems.c/hud.c use. The "HIGH" score's own 4-digit number reuses
// hud.c's exact digit bitmap data (hud_digit_tiles, exported via
// hud.h) at a separate tile-ID range below, rather than duplicating it.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "highscore.h"
#include "hud.h"
#include "title.h"

#define TITLE_TILE_BASE 15 // right after hud.c's digit range (5-14)
#define TITLE_BLANK (TITLE_TILE_BASE + 0)
#define TITLE_P (TITLE_TILE_BASE + 1)
#define TITLE_R (TITLE_TILE_BASE + 2)
#define TITLE_I (TITLE_TILE_BASE + 3)
#define TITLE_S (TITLE_TILE_BASE + 4)
#define TITLE_M (TITLE_TILE_BASE + 5)
#define TITLE_E (TITLE_TILE_BASE + 6)
#define TITLE_T (TITLE_TILE_BASE + 7)
#define TITLE_A (TITLE_TILE_BASE + 8)
#define TITLE_H (TITLE_TILE_BASE + 9)
#define TITLE_G (TITLE_TILE_BASE + 10)
#define TITLE_TILE_COUNT 11

// A separate tile-ID range for the "HIGH" line's own 4-digit number -
// title.c's own letter glyphs already run through TITLE_TILE_BASE+10,
// so this starts right after them.
#define TITLE_DIGIT_TILE_BASE (TITLE_TILE_BASE + TITLE_TILE_COUNT)

static const uint8_t title_tiles[TITLE_TILE_COUNT * 16] = {
    // blank/space
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    // P
    0xF0,0xF0, 0x88,0x88, 0x88,0x88, 0xF0,0xF0, 0x80,0x80, 0x80,0x80, 0x80,0x80, 0x00,0x00,
    // R
    0xF0,0xF0, 0x88,0x88, 0x88,0x88, 0xF0,0xF0, 0xA0,0xA0, 0x90,0x90, 0x88,0x88, 0x00,0x00,
    // I
    0x70,0x70, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x70,0x70, 0x00,0x00,
    // S
    0x78,0x78, 0x80,0x80, 0x80,0x80, 0x70,0x70, 0x08,0x08, 0x08,0x08, 0xF0,0xF0, 0x00,0x00,
    // M
    0x88,0x88, 0xD8,0xD8, 0xA8,0xA8, 0xA8,0xA8, 0x88,0x88, 0x88,0x88, 0x88,0x88, 0x00,0x00,
    // E
    0xF8,0xF8, 0x80,0x80, 0x80,0x80, 0xF0,0xF0, 0x80,0x80, 0x80,0x80, 0xF8,0xF8, 0x00,0x00,
    // T
    0xF8,0xF8, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x00,0x00,
    // A
    0x70,0x70, 0x88,0x88, 0x88,0x88, 0xF8,0xF8, 0x88,0x88, 0x88,0x88, 0x88,0x88, 0x00,0x00,
    // H
    0x88,0x88, 0x88,0x88, 0x88,0x88, 0xF8,0xF8, 0x88,0x88, 0x88,0x88, 0x88,0x88, 0x00,0x00,
    // G
    0x70,0x70, 0x88,0x88, 0x80,0x80, 0xB8,0xB8, 0x88,0x88, 0x88,0x88, 0x70,0x70, 0x00,0x00,
};

// Palette index 5 - gems.c uses 0-4. Color 0 a dark navy background,
// colors 1-3 a bright gold (only color 3 is actually lit by any
// letterform above, same "2-color-only shape" convention as gems.c/
// hud.c; 1-2 filled sensibly rather than left meaningless).
#define TITLE_PALETTE 5
static const palette_color_t title_palette[4] = {
    RGB(0, 0, 6), RGB(28, 22, 4), RGB(30, 24, 6), RGB(31, 27, 8),
};

#define SCREEN_TILE_W 32
#define SCREEN_TILE_H 32

// Same blank-fill shape as board.c's own file-local fill_screen_blank()
// - small enough, and different enough in purpose (title UI vs. grid
// state), that a shared header isn't worth it; each screen-drawing
// module here owns its own copy, same as cursor.c/hud.c each staying
// self-contained.
static void fill_screen_blank(void) {
    uint8_t blank_row[SCREEN_TILE_W];
    for (uint8_t i = 0; i < SCREEN_TILE_W; i++) blank_row[i] = TITLE_BLANK;
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        set_bkg_tiles(0, y, SCREEN_TILE_W, 1, blank_row);
    }
}

static uint8_t glyph_tile(char c) {
    switch (c) {
        case 'P': return TITLE_P;
        case 'R': return TITLE_R;
        case 'I': return TITLE_I;
        case 'S': return TITLE_S;
        case 'M': return TITLE_M;
        case 'E': return TITLE_E;
        case 'T': return TITLE_T;
        case 'A': return TITLE_A;
        case 'H': return TITLE_H;
        case 'G': return TITLE_G;
        default: return TITLE_BLANK; // space
    }
}

// Draws a short (<=20 char) string on one background row, horizontally
// centered, in TITLE_PALETTE.
static void draw_centered(const char *text, uint8_t len, uint8_t y) {
    uint8_t tiles[20];
    for (uint8_t i = 0; i < len; i++) tiles[i] = glyph_tile(text[i]);
    uint8_t x = (uint8_t)((20 - len) / 2);
    set_bkg_tiles(x, y, len, 1, tiles);

    uint8_t attrs[20];
    for (uint8_t i = 0; i < len; i++) attrs[i] = TITLE_PALETTE;
    set_bkg_attributes(x, y, len, 1, attrs);
}

// Same digit-extraction shape as hud.c's own draw_number(), but
// targeting TITLE_DIGIT_TILE_BASE (this module's own tile-ID range)
// and horizontally centered rather than fixed-position - hud.c's
// version isn't reusable as-is since it hardcodes HUD_DIGIT_TILE_BASE
// and a fixed x/y.
static void draw_number_centered(uint16_t value, uint8_t digits, uint8_t y) {
    uint8_t tiles[4]; // 4 digits is the largest field used (the score)
    for (int8_t i = (int8_t)digits - 1; i >= 0; i--) {
        tiles[i] = (uint8_t)(TITLE_DIGIT_TILE_BASE + (value % 10));
        value /= 10;
    }
    uint8_t x = (uint8_t)((20 - digits) / 2);
    set_bkg_tiles(x, y, digits, 1, tiles);

    uint8_t attrs[4];
    for (uint8_t i = 0; i < digits; i++) attrs[i] = TITLE_PALETTE;
    set_bkg_attributes(x, y, digits, 1, attrs);
}

void title_screen(void) {
    set_bkg_palette(TITLE_PALETTE, 1, title_palette);
    set_bkg_data(TITLE_TILE_BASE, TITLE_TILE_COUNT, title_tiles);
    set_bkg_data(TITLE_DIGIT_TILE_BASE, HUD_DIGIT_COUNT, hud_digit_tiles);
    fill_screen_blank();

    draw_centered("PRISM", 5, 6);
    draw_centered("PRESS START", 11, 10);
    draw_centered("HIGH", 4, 13);
    draw_number_centered(highscore_get(), 4, 15);

    SHOW_BKG;
    DISPLAY_ON;

    uint8_t prev_joy = 0;
    while (1) {
        uint8_t joy = joypad();
        uint8_t pressed = (uint8_t)(joy & (uint8_t)~prev_joy);
        prev_joy = joy;
        if (pressed & J_START) break;
        vsync();
    }

    // Real-hardware-safe LCD-disable timing (only during VBlank) -
    // see docs/GAMEBOY_ROADMAP.md's Phase 8 LCD-enable/disable timing
    // work - so main()'s own gem/HUD tile-data load never has a frame
    // where the tilemap points at these title tile IDs holding
    // freshly-overwritten gem/digit bitmap data instead.
    wait_vbl_done();
    DISPLAY_OFF;
}
