// See enemy.h. One 8x8 round blob (Manhattan/circular-distance
// generated - a per-pixel "is this within radius 3 of the tile
// center" check, same technique as every other tile asset in this
// project, converted to GB 2bpp planar tile bytes by a one-off local
// script), deliberately smaller/simpler than the 16x16 player - a
// weaker first enemy, not a scope shortfall. One CGB OBJ palette, a
// distinct "danger"-coded red/orange.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "enemy.h"

#define ENEMY_TILE 14 // player.c owns 0-11, sword.c owns 12-13
static const uint8_t enemy_tile[16] = {
    0x00, 0x00, 0x3C, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x3C, 0x00, 0x00,
};

#define ENEMY_PALETTE 2 // player.c owns 0, sword.c owns 1
static const palette_color_t enemy_palette[4] = {
    RGB(0, 0, 0), RGB(18, 4, 2), RGB(24, 6, 3), RGB(31, 9, 4),
};

#define ENEMY_SPRITE 5 // player.c owns 0-3, sword.c owns 4

// Fixed horizontal patrol range at a fixed y, comfortably inside room
// (0,0)'s open floor and away from any wall.
#define ENEMY_MIN_X 96
#define ENEMY_MAX_X 128
#define ENEMY_Y 64

static uint8_t enemy_x;
static int8_t enemy_dir;
static uint8_t alive;

static void position_enemy(void) {
    move_sprite(ENEMY_SPRITE, enemy_x + 8, ENEMY_Y + 16);
}

void enemy_hide(void) {
    move_sprite(ENEMY_SPRITE, 0, 0);
}

void enemy_init(void) {
    set_sprite_palette(ENEMY_PALETTE, 1, enemy_palette);
    set_sprite_data(ENEMY_TILE, 1, enemy_tile);
    set_sprite_tile(ENEMY_SPRITE, ENEMY_TILE);
    set_sprite_prop(ENEMY_SPRITE, S_PAL(ENEMY_PALETTE));

    enemy_x = ENEMY_MIN_X;
    enemy_dir = 1;
    alive = 1;
    position_enemy();
}

void enemy_update(void) {
    if (!alive) return;

    uint8_t new_x = (uint8_t)(enemy_x + enemy_dir);
    if (new_x < ENEMY_MIN_X || new_x > ENEMY_MAX_X) {
        enemy_dir = (int8_t)(-enemy_dir);
        new_x = (uint8_t)(enemy_x + enemy_dir);
    }
    enemy_x = new_x;

    position_enemy();
}

uint8_t enemy_try_hit(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (!alive) return 0;

    uint8_t overlap_x = hx < (uint8_t)(enemy_x + 8) && (uint8_t)(hx + hw) > enemy_x;
    uint8_t overlap_y = hy < (uint8_t)(ENEMY_Y + 8) && (uint8_t)(hy + hh) > ENEMY_Y;
    if (!overlap_x || !overlap_y) return 0;

    alive = 0;
    enemy_hide();
    return 1;
}

uint8_t enemy_get_x(void) { return enemy_x; }
uint8_t enemy_get_y(void) { return ENEMY_Y; }
uint8_t enemy_is_alive(void) { return alive; }
