// See cursor.h.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "cursor.h"
#include "grid.h"

// One 8x8 "corner bracket" - a right-angle highlight mark - reused for
// all four corners of a 16x16 highlight frame via hardware sprite flip
// flags (S_FLIPX/S_FLIPY) rather than four separate tiles. Color index
// 0 is hardware-transparent for sprites unconditionally (real GB
// behavior, not a palette choice), so no "background" color handling
// is needed the way the gem tiles (gems.c) needed.
#define CURSOR_TILE_ID 0
static const uint8_t cursor_tile[16] = {
    0xF8, 0xF8, 0xF8, 0xF8, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// One CGB OBJ palette - bright white/yellow, high-contrast against any
// of the five gem colors (gems.c). Color 0 is never actually rendered
// for a sprite (always transparent), so its value here is irrelevant -
// present only because the array needs 4 entries.
static const palette_color_t cursor_palette[4] = {
    RGB(0, 0, 0), RGB(31, 31, 10), RGB(31, 31, 20), RGB(31, 31, 31),
};

// Sprite numbers 0-3 (of the hardware's 40) - free to use, main.c draws
// no other sprites this milestone.
#define SPRITE_TL 0
#define SPRITE_TR 1
#define SPRITE_BL 2
#define SPRITE_BR 3

static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static uint8_t prev_joy = 0;

// GBDK's move_sprite() takes screen position +8 (x) / +16 (y), not raw
// screen pixels (gb/gb.h's own move_sprite() doc comment) - px/py below
// are already the real on-screen top-left pixel of the cursor cell.
static void position_sprites(void) {
    uint8_t px = GRID_ORIGIN_X * 8 + cursor_x * 16;
    uint8_t py = GRID_ORIGIN_Y * 8 + cursor_y * 16;

    move_sprite(SPRITE_TL, px + 8, py + 16);
    set_sprite_prop(SPRITE_TL, 0);

    move_sprite(SPRITE_TR, px + 8 + 8, py + 16);
    set_sprite_prop(SPRITE_TR, S_FLIPX);

    move_sprite(SPRITE_BL, px + 8, py + 16 + 8);
    set_sprite_prop(SPRITE_BL, S_FLIPY);

    move_sprite(SPRITE_BR, px + 8 + 8, py + 16 + 8);
    set_sprite_prop(SPRITE_BR, S_FLIPX | S_FLIPY);
}

void cursor_init(void) {
    set_sprite_palette(0, 1, cursor_palette);
    set_sprite_data(CURSOR_TILE_ID, 1, cursor_tile);

    set_sprite_tile(SPRITE_TL, CURSOR_TILE_ID);
    set_sprite_tile(SPRITE_TR, CURSOR_TILE_ID);
    set_sprite_tile(SPRITE_BL, CURSOR_TILE_ID);
    set_sprite_tile(SPRITE_BR, CURSOR_TILE_ID);

    cursor_x = 0;
    cursor_y = 0;
    prev_joy = 0;
    position_sprites();

    SHOW_SPRITES;
}

void cursor_update(uint8_t joy) {
    uint8_t pressed = (uint8_t)(joy & (uint8_t)~prev_joy); // newly pressed this frame only
    prev_joy = joy;

    if ((pressed & J_LEFT) && cursor_x > 0) cursor_x--;
    if ((pressed & J_RIGHT) && cursor_x < GRID_SIZE - 1) cursor_x++;
    if ((pressed & J_UP) && cursor_y > 0) cursor_y--;
    if ((pressed & J_DOWN) && cursor_y < GRID_SIZE - 1) cursor_y++;

    position_sprites();
}

uint8_t cursor_get_x(void) { return cursor_x; }
uint8_t cursor_get_y(void) { return cursor_y; }
