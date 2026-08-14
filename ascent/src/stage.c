// See stage.h. Four 8x8 BG tiles - empty (open air), floor (a
// horizontal girder), ladder (an open rung-and-rail segment), and
// ladder-top (a girder with a ladder passing through it, where a
// ladder run starts or ends at a platform) - each computed from a
// simple row-bitmask rule, same "identical low/high byte gives one
// solid color per row" convention every hand-drawn tile in this
// project's sibling games (prism/, wayfarer/) already relies on,
// verified by actually rendering this screen through this project's
// own emulator before being committed here.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "stage.h"

#define STAGE_TILE_COUNT 4

static const uint8_t stage_tiles[STAGE_TILE_COUNT * 16] = {
    // empty: solid color 0 (the backdrop)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // floor: dark rivet edge (color 1) top/bottom row, rust fill
    // (color 2) between - a chunky girder cross-section
    0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
    0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
    // ladder: a thin center rail (color 3, columns 3-4) on the
    // backdrop for 7 rows, then a full-width rung (color 3) on the
    // 8th - stacked ladder tiles read as evenly-spaced rungs
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF,
    // ladder-top: floor's own rivet edges top/bottom, but the fill
    // rows show the ladder rail (color 3, columns 3-4) instead of
    // solid color 2 - the ladder visibly passing through the girder
    0xFF, 0x00, 0x18, 0xFF, 0x18, 0xFF, 0x18, 0xFF,
    0x18, 0xFF, 0x18, 0xFF, 0x18, 0xFF, 0xFF, 0x00,
};

// One CGB BG palette: color 0 a dark navy backdrop, color 1 a dark
// rivet-brown girder edge, color 2 a warm rust-brown girder fill,
// color 3 a pale yellow ladder rail/rung.
#define STAGE_PALETTE 0
static const palette_color_t stage_palette[4] = {
    RGB(2, 2, 10), RGB(10, 6, 2), RGB(22, 14, 4), RGB(28, 26, 10),
};

// Four girder tiers (rows 5, 9, 13, 17), zigzagging ladders at
// columns 4 and 14 - see stage.h's own doc comment. Authored directly
// as a flat literal (generated once by a local script from the same
// row/column rules, then verified by rendering) rather than a
// generative loop like a bordered room would need, since this screen
// has no per-instance variation to parameterize.
static const uint8_t stage_map[STAGE_TILE_W * STAGE_TILE_H] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 1
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 2
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 3
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 4
    1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // row 5 (tier 3, top)
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 6
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 7
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 8
    1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, // row 9 (tier 2)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, // row 10
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, // row 11
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, // row 12
    1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, // row 13 (tier 1)
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 14
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 15
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // row 16
    1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // row 17 (ground)
};

void stage_init(void) {
    set_bkg_palette(STAGE_PALETTE, 1, stage_palette);
    set_bkg_data(EMPTY_TILE, STAGE_TILE_COUNT, stage_tiles);
    set_bkg_tiles(0, 0, STAGE_TILE_W, STAGE_TILE_H, stage_map);
}

uint8_t stage_tile_at(uint8_t px, uint8_t py) {
    uint8_t col = px / 8;
    uint8_t row = py / 8;
    if (col >= STAGE_TILE_W) col = STAGE_TILE_W - 1;
    if (row >= STAGE_TILE_H) row = STAGE_TILE_H - 1;
    return stage_map[(uint16_t)row * STAGE_TILE_W + col];
}

uint8_t stage_is_solid(uint8_t px, uint8_t py) {
    uint8_t tile = stage_tile_at(px, py);
    return tile == FLOOR_TILE || tile == LADDER_TOP_TILE;
}

uint8_t stage_is_ladder(uint8_t px, uint8_t py) {
    uint8_t tile = stage_tile_at(px, py);
    return tile == LADDER_TILE || tile == LADDER_TOP_TILE;
}
