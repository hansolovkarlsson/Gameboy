// See key.h. One hand-authored 8x8 key-shape tile (a ring, a shaft,
// one tooth - a key silhouette isn't formulaic like the diamond/circle
// shapes some other tile assets here used, so drawn directly as a
// per-pixel color-index grid, same "draw directly, verify by
// rendering" approach heart_hud.c's heart used). One CGB OBJ palette,
// gold.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "key.h"
#include "room.h"

#define KEY_TILE_ID 16 // player.c owns 0-11, sword.c owns 12-13, enemy.c owns 14, heart_hud.c owns 15
static const uint8_t key_tile[16] = {
    0x38, 0x38, 0x6C, 0x6C, 0x38, 0x38, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x38, 0x38, 0x00, 0x00,
};

#define KEY_PALETTE 5 // player.c owns 0, sword.c owns 1, enemy.c owns 2, heart_hud.c owns 3-4
static const palette_color_t key_palette[4] = {
    RGB(0, 0, 0), RGB(20, 16, 2), RGB(26, 21, 3), RGB(31, 26, 4),
};

#define KEY_SPRITE 10 // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, heart_hud.c owns 6-8, pickup.c owns 9

static uint8_t collected;

void key_hide(void) {
    move_sprite(KEY_SPRITE, 0, 0);
}

void key_show(void) {
    if (collected) return;
    set_sprite_tile(KEY_SPRITE, KEY_TILE_ID);
    set_sprite_prop(KEY_SPRITE, S_PAL(KEY_PALETTE));
    move_sprite(KEY_SPRITE, ROOM_CENTER_X + 8, ROOM_CENTER_Y + 16);
}

void key_init(void) {
    set_sprite_palette(KEY_PALETTE, 1, key_palette);
    set_sprite_data(KEY_TILE_ID, 1, key_tile);
    collected = 0;
    key_hide(); // the game starts in room (0,0), not this key's room (1,0)
}

uint8_t key_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (collected) return 0;

    uint8_t overlap_x = hx < (uint8_t)(ROOM_CENTER_X + 8) && (uint8_t)(hx + hw) > ROOM_CENTER_X;
    uint8_t overlap_y = hy < (uint8_t)(ROOM_CENTER_Y + 8) && (uint8_t)(hy + hh) > ROOM_CENTER_Y;
    if (!overlap_x || !overlap_y) return 0;

    collected = 1;
    key_hide();
    return 1;
}

uint8_t key_is_collected(void) { return collected; }
