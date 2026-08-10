// See player.h. Three directional 16x16 (2x2-tile) icons - down
// (front-facing), up (back-facing), and one side profile reused
// mirrored via S_FLIPX for left vs right (same "one tile, four flipped
// corners" reuse discipline the sibling prism/ project already
// established for its cursor/swap-animation sprites, applied here as
// "one profile, two flipped facings" instead) - 12 unique 8x8 tiles
// total, a simple recognizable humanoid shape, not detailed art. Each
// generated from a per-pixel color-index grid by a one-off local
// script (same technique as every tile asset in prism/), verified by
// actually rendering it through this project's own emulator before
// being committed here.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "player.h"
#include "room.h"

#define TILE_DOWN_TL 0
#define TILE_DOWN_TR 1
#define TILE_DOWN_BL 2
#define TILE_DOWN_BR 3
#define TILE_UP_TL 4
#define TILE_UP_TR 5
#define TILE_UP_BL 6
#define TILE_UP_BR 7
#define TILE_SIDE_TL 8
#define TILE_SIDE_TR 9
#define TILE_SIDE_BL 10
#define TILE_SIDE_BR 11
#define PLAYER_TILE_COUNT 12

static const uint8_t player_tiles[PLAYER_TILE_COUNT * 16] = {
    // down: top-left
    0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x02, 0x0D,
    0x0F, 0x0F, 0x0F, 0x3F, 0x0F, 0x3F, 0x0F, 0x3F,
    // down: top-right
    0xF0, 0x00, 0xF0, 0x00, 0x00, 0xF0, 0x40, 0xB0,
    0xF0, 0xF0, 0xC0, 0xF0, 0xC0, 0xF0, 0xC0, 0xF0,
    // down: bottom-left
    0x0F, 0x3F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x00, 0x0E, 0x00, 0x0E, 0x00, 0x0E, 0x00, 0x0E,
    // down: bottom-right
    0xC0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70,

    // up: top-left
    0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00,
    0x0F, 0x0F, 0x0F, 0x3F, 0x0F, 0x3F, 0x0F, 0x3F,
    // up: top-right
    0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00, 0xF0, 0x00,
    0xF0, 0xF0, 0xC0, 0xF0, 0xC0, 0xF0, 0xC0, 0xF0,
    // up: bottom-left
    0x0F, 0x3F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x00, 0x0E, 0x00, 0x0E, 0x00, 0x0E, 0x00, 0x0E,
    // up: bottom-right
    0xC0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70,

    // side (facing right): top-left
    0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x00, 0x0F,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    // side (facing right): top-right
    0xF0, 0x00, 0xF0, 0x00, 0x00, 0xF0, 0x40, 0xB0,
    0xF0, 0xF0, 0xF0, 0xFC, 0xF0, 0xFC, 0xF0, 0xFC,
    // side (facing right): bottom-left
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x00, 0x0E, 0x00, 0x0E, 0x00, 0x0E, 0x00, 0x0E,
    // side (facing right): bottom-right
    0xF0, 0xFC, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0x00, 0x70, 0x00, 0x70, 0x00, 0x70, 0x00, 0x70,
};

// One CGB OBJ palette: color 0 is unused (hardware-transparent for
// sprites unconditionally, same as prism/'s cursor.c note), color 1 a
// dark brown (hair/outline), color 2 a warm skin tone, color 3 a blue
// tunic.
#define PLAYER_PALETTE 0
static const palette_color_t player_palette[4] = {
    RGB(0, 0, 0), RGB(12, 7, 3), RGB(28, 20, 14), RGB(4, 10, 26),
};

// Sprite slots 0-3 - the only sprites this project draws yet.
#define SPRITE_TL 0
#define SPRITE_TR 1
#define SPRITE_BL 2
#define SPRITE_BR 3

typedef enum {
    FACING_DOWN,
    FACING_UP,
    FACING_LEFT,
    FACING_RIGHT,
} facing_t;

static uint8_t player_x;
static uint8_t player_y;
static facing_t facing;

#define MAX_HEARTS 3
// ~1s at ~59.7fps - long enough that a single enemy pass (it takes
// only ~24 frames to fully cross a stationary 16px-wide player at
// 1px/frame) can't cause a second deduction from the same graze.
#define INVINCIBILITY_FRAMES 60

static uint8_t hearts = MAX_HEARTS;
static uint8_t invincible_timer = 0;

// Positions the 4 quadrant sprites (cursor.c's own position_sprites()
// pattern, sibling prism/ project) so the combined 16x16 icon's
// top-left pixel is at (player_x, player_y), picking whichever
// direction's tile set applies. FACING_LEFT reuses the side-facing
// (right) tile set with both quadrant slots *and* tile assignment
// swapped left<->right and S_FLIPX set on all four - mirroring the
// whole icon, not just each individual tile.
static void position_player(void) {
    uint8_t px = player_x;
    uint8_t py = player_y;
    uint8_t tl, tr, bl, br, flip;

    switch (facing) {
        case FACING_UP:
            tl = TILE_UP_TL; tr = TILE_UP_TR; bl = TILE_UP_BL; br = TILE_UP_BR;
            flip = 0;
            break;
        case FACING_LEFT:
            tl = TILE_SIDE_TR; tr = TILE_SIDE_TL; bl = TILE_SIDE_BR; br = TILE_SIDE_BL;
            flip = S_FLIPX;
            break;
        case FACING_RIGHT:
            tl = TILE_SIDE_TL; tr = TILE_SIDE_TR; bl = TILE_SIDE_BL; br = TILE_SIDE_BR;
            flip = 0;
            break;
        case FACING_DOWN:
        default:
            tl = TILE_DOWN_TL; tr = TILE_DOWN_TR; bl = TILE_DOWN_BL; br = TILE_DOWN_BR;
            flip = 0;
            break;
    }

    set_sprite_tile(SPRITE_TL, tl);
    set_sprite_prop(SPRITE_TL, flip);
    move_sprite(SPRITE_TL, px + 8, py + 16);

    set_sprite_tile(SPRITE_TR, tr);
    set_sprite_prop(SPRITE_TR, flip);
    move_sprite(SPRITE_TR, px + 8 + 8, py + 16);

    set_sprite_tile(SPRITE_BL, bl);
    set_sprite_prop(SPRITE_BL, flip);
    move_sprite(SPRITE_BL, px + 8, py + 16 + 8);

    set_sprite_tile(SPRITE_BR, br);
    set_sprite_prop(SPRITE_BR, flip);
    move_sprite(SPRITE_BR, px + 8 + 8, py + 16 + 8);
}

void player_init(void) {
    set_sprite_palette(PLAYER_PALETTE, 1, player_palette);
    set_sprite_data(TILE_DOWN_TL, PLAYER_TILE_COUNT, player_tiles);

    player_x = ROOM_CENTER_X;
    player_y = ROOM_CENTER_Y;
    facing = FACING_DOWN;
    hearts = MAX_HEARTS;
    invincible_timer = 0;

    position_player();
    SHOW_SPRITES;
}

void player_update(uint8_t joy) {
    int8_t dx = 0;
    int8_t dy = 0;

    // Fixed checking order, each later match overwriting facing/delta -
    // gives a simple, deterministic diagonal tie-break (down beats up
    // beats right beats left) rather than tracking true press-order
    // timing, which would need extra per-direction edge-timestamp state
    // this milestone has no other use for.
    if (joy & J_LEFT) { dx = -1; facing = FACING_LEFT; }
    if (joy & J_RIGHT) { dx = 1; facing = FACING_RIGHT; }
    if (joy & J_UP) { dy = -1; facing = FACING_UP; }
    if (joy & J_DOWN) { dy = 1; facing = FACING_DOWN; }

    if (dx != 0) {
        uint8_t new_x = (uint8_t)(player_x + dx);
        if (!room_blocks(new_x, player_y)) player_x = new_x;
    }
    if (dy != 0) {
        uint8_t new_y = (uint8_t)(player_y + dy);
        if (!room_blocks(player_x, new_y)) player_y = new_y;
    }

    if (invincible_timer > 0) invincible_timer--;

    position_player();
}

uint8_t player_get_x(void) { return player_x; }
uint8_t player_get_y(void) { return player_y; }
uint8_t player_get_facing(void) { return (uint8_t)facing; }

void player_set_position(uint8_t x, uint8_t y) {
    player_x = x;
    player_y = y;
    position_player();
}

uint8_t player_get_hearts(void) { return hearts; }

uint8_t player_damage(uint8_t amount) {
    if (invincible_timer > 0) return 0;
    hearts = (amount >= hearts) ? 0 : (uint8_t)(hearts - amount);
    invincible_timer = INVINCIBILITY_FRAMES;
    return 1;
}

void player_heal_full(void) {
    hearts = MAX_HEARTS;
}
