// See title.h. A small custom letter tileset - only the 9 distinct
// letters actually needed across "WAYFARER" and "PRESS START" (W, A,
// Y, F, R, E, P, S, T) plus a blank/space tile, the same "generate
// programmatically, verify visually" 5x7 block-letter approach the
// sibling prism/ project's own title.c and this project's own win.c
// (Milestone 6) already established - the third application of the
// same proven pattern, not a new one. Independently derived, but
// verified to match prism/title.c's own P/R/E/S/T/A bytes and win.c's
// own W bytes exactly (the same standard block-font convention, not
// copied art).
//
// Own, non-overlapping BG tile range (6 - room.c owns 0-2, win.c owns
// 3-5) and own blank tile - title_screen() runs *before* room_init()
// ever loads FLOOR_TILE_ID's real data, so reusing it here (the way
// win.c safely does, since it runs after room_init()) would render
// undefined VRAM content. prism/'s own title.c has this same
// constraint and handles it the same way.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "title.h"

#define TITLE_TILE_BASE 6
#define TITLE_BLANK (TITLE_TILE_BASE + 0)
#define TITLE_W (TITLE_TILE_BASE + 1)
#define TITLE_A (TITLE_TILE_BASE + 2)
#define TITLE_Y (TITLE_TILE_BASE + 3)
#define TITLE_F (TITLE_TILE_BASE + 4)
#define TITLE_R (TITLE_TILE_BASE + 5)
#define TITLE_E (TITLE_TILE_BASE + 6)
#define TITLE_P (TITLE_TILE_BASE + 7)
#define TITLE_S (TITLE_TILE_BASE + 8)
#define TITLE_T (TITLE_TILE_BASE + 9)
#define TITLE_TILE_COUNT 10

static const uint8_t title_tiles[TITLE_TILE_COUNT * 16] = {
    // blank/space
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    // W
    0x88,0x88, 0x88,0x88, 0x88,0x88, 0xA8,0xA8, 0xA8,0xA8, 0xD8,0xD8, 0x88,0x88, 0x00,0x00,
    // A
    0x70,0x70, 0x88,0x88, 0x88,0x88, 0xF8,0xF8, 0x88,0x88, 0x88,0x88, 0x88,0x88, 0x00,0x00,
    // Y
    0x88,0x88, 0x88,0x88, 0x50,0x50, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x00,0x00,
    // F
    0xF8,0xF8, 0x80,0x80, 0x80,0x80, 0xF0,0xF0, 0x80,0x80, 0x80,0x80, 0x80,0x80, 0x00,0x00,
    // R
    0xF0,0xF0, 0x88,0x88, 0x88,0x88, 0xF0,0xF0, 0xA0,0xA0, 0x90,0x90, 0x88,0x88, 0x00,0x00,
    // E
    0xF8,0xF8, 0x80,0x80, 0x80,0x80, 0xF0,0xF0, 0x80,0x80, 0x80,0x80, 0xF8,0xF8, 0x00,0x00,
    // P
    0xF0,0xF0, 0x88,0x88, 0x88,0x88, 0xF0,0xF0, 0x80,0x80, 0x80,0x80, 0x80,0x80, 0x00,0x00,
    // S
    0x78,0x78, 0x80,0x80, 0x80,0x80, 0x70,0x70, 0x08,0x08, 0x08,0x08, 0xF0,0xF0, 0x00,0x00,
    // T
    0xF8,0xF8, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x20,0x20, 0x00,0x00,
};

// Palette index 3 - room.c owns 0 (room) and 1 (door), win.c owns 2.
// A cool blue, distinct from all three.
#define TITLE_PALETTE 3
static const palette_color_t title_palette[4] = {
    RGB(2, 3, 10), RGB(6, 10, 22), RGB(10, 16, 27), RGB(16, 22, 31),
};

#define SCREEN_TILE_W 20
#define SCREEN_TILE_H 18

static void fill_screen_blank(void) {
    uint8_t blank_row[SCREEN_TILE_W];
    uint8_t blank_attrs[SCREEN_TILE_W];
    for (uint8_t i = 0; i < SCREEN_TILE_W; i++) {
        blank_row[i] = TITLE_BLANK;
        blank_attrs[i] = TITLE_PALETTE;
    }
    for (uint8_t y = 0; y < SCREEN_TILE_H; y++) {
        set_bkg_tiles(0, y, SCREEN_TILE_W, 1, blank_row);
        set_bkg_attributes(0, y, SCREEN_TILE_W, 1, blank_attrs);
    }
}

static uint8_t glyph_tile(char c) {
    switch (c) {
        case 'W': return TITLE_W;
        case 'A': return TITLE_A;
        case 'Y': return TITLE_Y;
        case 'F': return TITLE_F;
        case 'R': return TITLE_R;
        case 'E': return TITLE_E;
        case 'P': return TITLE_P;
        case 'S': return TITLE_S;
        case 'T': return TITLE_T;
        default: return TITLE_BLANK; // space
    }
}

// Draws a short (<=20 char) string on one background row, horizontally
// centered, in TITLE_PALETTE.
static void draw_centered(const char *text, uint8_t len, uint8_t y) {
    uint8_t tiles[20];
    for (uint8_t i = 0; i < len; i++) tiles[i] = glyph_tile(text[i]);
    uint8_t x = (uint8_t)((SCREEN_TILE_W - len) / 2);
    set_bkg_tiles(x, y, len, 1, tiles);

    uint8_t attrs[20];
    for (uint8_t i = 0; i < len; i++) attrs[i] = TITLE_PALETTE;
    set_bkg_attributes(x, y, len, 1, attrs);
}

void title_screen(void) {
    // Real CGB hardware's boot ROM leaves the LCD *on* (LCDC=$91) with
    // every background palette color white before handing off control
    // (pandocs' Power_Up_Sequence.md) - disabling the display before
    // any bulk VRAM/tilemap/palette write below avoids a real,
    // well-known GB homebrew gotcha (a live LCD catching a write
    // mid-flight, producing a torn frame), same real-hardware-safe
    // (VBlank-gated) LCD-disable pattern the sibling prism/ project's
    // own title.c and this project's own main.c/world.c already use.
    wait_vbl_done();
    DISPLAY_OFF;

    set_bkg_palette(TITLE_PALETTE, 1, title_palette);
    set_bkg_data(TITLE_TILE_BASE, TITLE_TILE_COUNT, title_tiles);
    fill_screen_blank();

    draw_centered("WAYFARER", 8, 6);
    draw_centered("PRESS START", 11, 10);

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

    // Real-hardware-safe LCD-disable timing again, so world_init()'s
    // own subsequent tile loading never lands on a live display.
    wait_vbl_done();
    DISPLAY_OFF;
}
