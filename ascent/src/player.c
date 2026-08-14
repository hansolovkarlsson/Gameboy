// See player.h. One 16x16 side-profile icon (4 8x8 tiles, drawn
// facing right, mirrored via S_FLIPX for left) - the exact "one
// profile, two flipped facings" 4-quadrant sprite technique the
// sibling wayfarer/src/player.c already proved out (same tile art,
// reused verbatim rather than redrawn - a genuinely different game,
// but no reason to duplicate already-working CGB sprite art/
// positioning code for a plain humanoid figure).
//
// Physics: mostly a stateless per-frame check against stage.h's tile
// map - no "is_climbing" flag. Each frame: if the tile at the
// player's *feet* (the same point the solid/gravity check below uses)
// is a ladder tile *and* Up/Down is held, move vertically by 1px and
// skip gravity/walking entirely (this naturally self-limits - once
// the feet step past the ladder tile into open air or a plain floor
// tile, the very next frame's check no longer grants a climb, and
// gravity resumes). Otherwise: check that same feet point for
// solidity - solid (floor or ladder-top) means grounded (snap to rest
// exactly on top of it, and allow Left/Right); not solid means fall
// 1px.
//
// Milestone 2 adds one genuinely stateful action on top of that: a
// jump (see `jumping`/`jump_timer` below) is a timed forced-rise, not
// a tile condition, so unlike gravity/ladder-grip it can't be
// rederived fresh from the map every frame - it has to remember it's
// in progress.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "player.h"
#include "stage.h"

#define TILE_SIDE_TL 0
#define TILE_SIDE_TR 1
#define TILE_SIDE_BL 2
#define TILE_SIDE_BR 3
#define PLAYER_TILE_COUNT 4

static const uint8_t player_tiles[PLAYER_TILE_COUNT * 16] = {
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

// One CGB OBJ palette: color 0 unused (hardware-transparent for
// sprites unconditionally), color 1 a dark brown (hair/outline),
// color 2 a warm skin tone, color 3 a blue tunic - same values as
// wayfarer/src/player.c's own PLAYER_PALETTE, reused for the same
// reason as the tile art above.
#define PLAYER_PALETTE 0
static const palette_color_t player_palette[4] = {
    RGB(0, 0, 0), RGB(12, 7, 3), RGB(28, 20, 14), RGB(4, 10, 26),
};

// Sprite slots 0-3 - the only sprites this game draws yet.
#define SPRITE_TL 0
#define SPRITE_TR 1
#define SPRITE_BL 2
#define SPRITE_BR 3

typedef enum {
    FACING_LEFT,
    FACING_RIGHT,
} facing_t;

static uint8_t player_x;
static uint8_t player_y;
static facing_t facing;

// Jump state - see the header comment above. jumping is 0 whenever the
// player is grounded or gripping a ladder; jump_timer counts down the
// forced-rise frames, then the same 1px/frame fall as ordinary gravity
// carries the player back down until landing clears both.
static uint8_t jumping;
static uint8_t jump_timer;

// 20px of forced rise. A first attempt at 12px (checked against just
// the static height of an 8px barrel) turned out not to survive
// contact with a real, moving one: an approaching 8px barrel and a
// 16px-wide player overlap horizontally for ~22-24 frames as it
// closes in, but a 12px hop is only high enough to clear it (player's
// own bottom edge above the barrel's own top edge) for about 9 frames
// around its peak - nowhere near enough to cover the full approach,
// confirmed directly against the real build (a scripted jump timed at
// several different frames against an actual rolling barrel was
// caught every time). 20px widens that clearance window to ~21-25
// frames - enough to fully cover a barrel's real horizontal approach,
// not just its static footprint.
#define JUMP_RISE_FRAMES 20

#define PLAYER_MIN_X 0
#define PLAYER_MAX_X (160 - 16)

// Ground girder is tile row 17 (see stage.c) - resting on top of it
// means the sprite's bottom edge sits exactly at row 17's top pixel.
#define GROUND_TILE_ROW 17
#define GROUND_REST_Y (GROUND_TILE_ROW * 8 - 16)

// Spawn 8px above true rest height: the first several frames of
// gravity pulling the player down onto the girder is itself a free,
// automatic proof that landing/gravity works, with no dedicated
// checkpoint needed.
#define SPAWN_X 80
#define SPAWN_Y (GROUND_REST_Y - 8)

static void position_player(void) {
    uint8_t px = player_x;
    uint8_t py = player_y;
    uint8_t tl, tr, bl, br, flip;

    if (facing == FACING_LEFT) {
        tl = TILE_SIDE_TR; tr = TILE_SIDE_TL; bl = TILE_SIDE_BR; br = TILE_SIDE_BL;
        flip = S_FLIPX;
    } else {
        tl = TILE_SIDE_TL; tr = TILE_SIDE_TR; bl = TILE_SIDE_BL; br = TILE_SIDE_BR;
        flip = 0;
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
    set_sprite_data(TILE_SIDE_TL, PLAYER_TILE_COUNT, player_tiles);

    player_x = SPAWN_X;
    player_y = SPAWN_Y;
    facing = FACING_RIGHT;
    jumping = 0;
    jump_timer = 0;

    position_player();
    SHOW_SPRITES;
}

void player_update(uint8_t joy) {
    uint8_t center_x = player_x + 8;
    uint8_t feet_x = player_x + 8;
    uint8_t feet_y = player_y + 16;

    // Grip check uses the *feet* point - the same offset the solid/
    // gravity check below already uses - not the box's own vertical
    // center. Checking center happens to work for grabbing a ladder
    // from a dead stop, since standing at rest already means the top
    // two-thirds of the player's own box overlaps whatever shaft
    // leads *up* from here - but it never overlaps a shaft leading
    // *down* from here (that shaft starts on the far side of the
    // solid tile the player is standing on). The feet point is
    // exactly the tile being stood on, so it grants a grip in either
    // direction symmetrically, and it's the same point the mid-shaft
    // climb already re-checks every frame while moving, so this
    // doesn't change that part of the behavior at all.
    if ((joy & (J_UP | J_DOWN)) && stage_is_ladder(center_x, feet_y)) {
        if (joy & J_UP) {
            if (player_y > 0) player_y--;
        } else if (player_y < GROUND_REST_Y) {
            // Clamped explicitly rather than relying on the tile
            // check alone: stage_tile_at() clamps any row past the
            // map's own last row to that last row, so without this,
            // the ground's own ladder-top tile would keep reading as
            // "ladder" forever below the visible floor and the player
            // would sink out of the map holding Down.
            player_y++;
        }
        position_player();
        return;
    }
    uint8_t on_ground = stage_is_solid(feet_x, feet_y);

    if (!jumping && on_ground && (joy & J_A)) {
        jumping = 1;
        jump_timer = JUMP_RISE_FRAMES;
    }

    if (jumping) {
        if (jump_timer > 0) {
            player_y--;
            jump_timer--;
        } else {
            player_y++;
        }

        if (joy & J_LEFT) {
            facing = FACING_LEFT;
            if (player_x > PLAYER_MIN_X) player_x--;
        }
        if (joy & J_RIGHT) {
            facing = FACING_RIGHT;
            if (player_x < PLAYER_MAX_X) player_x++;
        }

        feet_y = player_y + 16;
        if (jump_timer == 0 && stage_is_solid(feet_x, feet_y)) {
            uint8_t tile_row = feet_y / 8;
            player_y = (uint8_t)(tile_row * 8 - 16);
            jumping = 0;
        }
    } else if (on_ground) {
        uint8_t tile_row = feet_y / 8;
        player_y = (uint8_t)(tile_row * 8 - 16);

        if (joy & J_LEFT) {
            facing = FACING_LEFT;
            if (player_x > PLAYER_MIN_X) player_x--;
        }
        if (joy & J_RIGHT) {
            facing = FACING_RIGHT;
            if (player_x < PLAYER_MAX_X) player_x++;
        }
    } else {
        player_y++;
    }

    position_player();
}

uint8_t player_get_x(void) { return player_x; }
uint8_t player_get_y(void) { return player_y; }

void player_respawn(void) {
    player_x = SPAWN_X;
    player_y = SPAWN_Y;
    jumping = 0;
    jump_timer = 0;
    position_player();
}
