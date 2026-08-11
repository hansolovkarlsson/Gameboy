// See sword.h. Two 8x8 tiles - a vertical bar (facing up/down) and a
// horizontal bar (facing left/right); a straight bar is already
// symmetric under the corresponding flip, so up and down (or left and
// right) share the same tile art with no flip logic needed. Generated
// from a per-pixel color-index grid by a one-off local script, same
// technique as every tile asset in this project, verified by actually
// rendering it before being committed here.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "player.h"
#include "sword.h"

#define TILE_HORIZONTAL 13 // TILE_VERTICAL is sword.h's own public SWORD_TILE_ID (12)
#define SWORD_TILE_COUNT 2

static const uint8_t sword_tiles[SWORD_TILE_COUNT * 16] = {
    // vertical bar (up/down)
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C,
    // horizontal bar (left/right)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFE,
    0xFE, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Bright silver, distinct from the player's own blue/tan/brown and
// (later) the enemy's red. Palette index itself is sword.h's own
// public SWORD_OBJ_PALETTE (1) - player.c owns 0.
static const palette_color_t sword_palette[4] = {
    RGB(0, 0, 0), RGB(20, 20, 22), RGB(26, 26, 28), RGB(31, 31, 31),
};

#define SWORD_SPRITE 4 // player.c owns sprite slots 0-3

// How long the blade sprite stays out, in frames - ~200ms at ~59.7fps,
// long enough to be clearly visible and to give a walking player a
// real window to land a hit, tuned by eye (same discipline as every
// other animation-duration constant in this project).
#define SWORD_FRAMES 12

static uint8_t timer = 0;
static uint8_t hitbox_x = 0;
static uint8_t hitbox_y = 0;

// One tile-width beyond whichever edge of the 16x16 player the facing
// points at, centered on the other axis ((16-8)/2 = 4). Stores the
// result in hitbox_x/hitbox_y (sword_get_x()/get_y()'s own backing
// state) so the hit-test position can never drift from the sprite's
// actual drawn position.
static void position_sword(void) {
    uint8_t px = player_get_x();
    uint8_t py = player_get_y();
    uint8_t facing = player_get_facing();
    uint8_t tile;

    switch (facing) {
        case PLAYER_FACING_UP:
            hitbox_x = (uint8_t)(px + 4);
            hitbox_y = (uint8_t)(py - 8);
            tile = SWORD_TILE_ID;
            break;
        case PLAYER_FACING_LEFT:
            hitbox_x = (uint8_t)(px - 8);
            hitbox_y = (uint8_t)(py + 4);
            tile = TILE_HORIZONTAL;
            break;
        case PLAYER_FACING_RIGHT:
            hitbox_x = (uint8_t)(px + 16);
            hitbox_y = (uint8_t)(py + 4);
            tile = TILE_HORIZONTAL;
            break;
        case PLAYER_FACING_DOWN:
        default:
            hitbox_x = (uint8_t)(px + 4);
            hitbox_y = (uint8_t)(py + 16);
            tile = SWORD_TILE_ID;
            break;
    }

    set_sprite_tile(SWORD_SPRITE, tile);
    move_sprite(SWORD_SPRITE, hitbox_x + 8, hitbox_y + 16);
}

static void hide_sword(void) {
    move_sprite(SWORD_SPRITE, 0, 0);
}

void sword_init(void) {
    set_sprite_palette(SWORD_OBJ_PALETTE, 1, sword_palette);
    set_sprite_data(SWORD_TILE_ID, SWORD_TILE_COUNT, sword_tiles);
    set_sprite_prop(SWORD_SPRITE, S_PAL(SWORD_OBJ_PALETTE));
    timer = 0;
    hide_sword();
}

void sword_update(uint8_t pressed_a) {
    if (pressed_a && timer == 0) {
        timer = SWORD_FRAMES;
    }

    if (timer == 0) return;

    position_sword();
    timer--;
    if (timer == 0) hide_sword();
}

uint8_t sword_is_active(void) {
    return timer > 0;
}

uint8_t sword_get_x(void) { return hitbox_x; }
uint8_t sword_get_y(void) { return hitbox_y; }
