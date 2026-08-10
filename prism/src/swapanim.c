// See swapanim.h.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "gems.h"
#include "grid.h"
#include "swapanim.h"

// cursor.c owns sprite tile ID 0 and OBJ palette 0; main.c's selection
// marker owns sprite tile ID 1 and OBJ palette 1 - these are the next
// free ranges (see swapanim.h's own note on why sprite/BG tile IDs
// don't collide here).
#define SWAPANIM_TILE_BASE 2
#define SWAPANIM_PALETTE_BASE 2

// cursor.c owns sprite slots 0-3, main.c's marker owns slot 4.
#define SPRITE_A_BASE 5
#define SPRITE_B_BASE 9

// 16px (one grid cell) / 8 frames = 2px/frame, an exact divisor - no
// rounding remainder to accumulate over the slide. ~134ms at ~59.7fps -
// fast enough to feel responsive, slow enough to actually see the two
// gems trade places (visually confirmed - see docs/GAMEBOY_ROADMAP.md's
// Phase 10 Milestone 7 entry).
#define SWAP_ANIM_FRAMES 8

// Positions one gem's 4 quadrant sprites (base sprite slot `base`) so
// their combined 16x16 icon's top-left pixel is at (px,py) - same
// TL/TR/BL/BR-via-flip-flags layout and move_sprite() +8/+16 offset
// convention as cursor.c's own position_sprites().
static void position_gem(uint8_t base, uint8_t gem, uint8_t px, uint8_t py) {
    uint8_t pal = S_PAL(SWAPANIM_PALETTE_BASE + gem); // CGB OBJ palette select (bits 0-2)

    set_sprite_tile(base, SWAPANIM_TILE_BASE); // top-left
    set_sprite_prop(base, pal);
    move_sprite(base, px + 8, py + 16);

    set_sprite_tile(base + 1, SWAPANIM_TILE_BASE); // top-right
    set_sprite_prop(base + 1, (uint8_t)(pal | S_FLIPX));
    move_sprite(base + 1, px + 8 + 8, py + 16);

    set_sprite_tile(base + 2, SWAPANIM_TILE_BASE); // bottom-left
    set_sprite_prop(base + 2, (uint8_t)(pal | S_FLIPY));
    move_sprite(base + 2, px + 8, py + 16 + 8);

    set_sprite_tile(base + 3, SWAPANIM_TILE_BASE); // bottom-right
    set_sprite_prop(base + 3, (uint8_t)(pal | S_FLIPX | S_FLIPY));
    move_sprite(base + 3, px + 8 + 8, py + 16 + 8);
}

static void hide_gem(uint8_t base) {
    move_sprite(base, 0, 0);
    move_sprite(base + 1, 0, 0);
    move_sprite(base + 2, 0, 0);
    move_sprite(base + 3, 0, 0);
}

// Blanks one 2x2-tile gem cell's background - same BLANK_TILE_ID/
// attribute-0 pattern board.c's own fill_screen_blank() uses, just
// scoped to a single cell instead of the whole screen.
static void blank_cell(uint8_t gx, uint8_t gy) {
    uint8_t tiles[2] = { BLANK_TILE_ID, BLANK_TILE_ID };
    uint8_t attrs[2] = { 0, 0 };
    uint8_t tx = GRID_ORIGIN_X + gx * 2;
    uint8_t ty = GRID_ORIGIN_Y + gy * 2;
    set_bkg_tiles(tx, ty, 2, 1, tiles);
    set_bkg_attributes(tx, ty, 2, 1, attrs);
    set_bkg_tiles(tx, ty + 1, 2, 1, tiles);
    set_bkg_attributes(tx, ty + 1, 2, 1, attrs);
}

void swapanim_init(void) {
    set_sprite_data(SWAPANIM_TILE_BASE, 1, gem_tiles); // one shared quadrant shape, recolored per gem via OBJ palette - see gems.h's own note on this technique
    set_sprite_palette(SWAPANIM_PALETTE_BASE, GEM_TYPE_COUNT, gem_palettes);
}

void swapanim_play(uint8_t from_x, uint8_t from_y, uint8_t gem_from,
                    uint8_t to_x, uint8_t to_y, uint8_t gem_to) {
    blank_cell(from_x, from_y);
    blank_cell(to_x, to_y);

    int16_t from_px = GRID_ORIGIN_X * 8 + from_x * 16;
    int16_t from_py = GRID_ORIGIN_Y * 8 + from_y * 16;
    int16_t to_px = GRID_ORIGIN_X * 8 + to_x * 16;
    int16_t to_py = GRID_ORIGIN_Y * 8 + to_y * 16;

    int16_t dx = (to_px - from_px) / SWAP_ANIM_FRAMES;
    int16_t dy = (to_py - from_py) / SWAP_ANIM_FRAMES;

    int16_t ax = from_px, ay = from_py; // gem_from's current position
    int16_t bx = to_px, by = to_py;     // gem_to's current position

    position_gem(SPRITE_A_BASE, gem_from, (uint8_t)ax, (uint8_t)ay);
    position_gem(SPRITE_B_BASE, gem_to, (uint8_t)bx, (uint8_t)by);

    for (uint8_t f = 0; f < SWAP_ANIM_FRAMES; f++) {
        ax = (int16_t)(ax + dx);
        ay = (int16_t)(ay + dy);
        bx = (int16_t)(bx - dx);
        by = (int16_t)(by - dy);
        position_gem(SPRITE_A_BASE, gem_from, (uint8_t)ax, (uint8_t)ay);
        position_gem(SPRITE_B_BASE, gem_to, (uint8_t)bx, (uint8_t)by);
        vsync();
    }

    hide_gem(SPRITE_A_BASE);
    hide_gem(SPRITE_B_BASE);
}
