// See sword_pickup.h. Reuses sword.c's own blade tile/palette directly
// (SWORD_TILE_ID/SWORD_OBJ_PALETTE, exported via sword.h) rather than
// drawing a second copy of the same art - the pickup icon is literally
// the same sword the player will go on to swing. Needs its own sprite
// OAM slot (a fixed on-ground icon, independent of the active-swing
// blade sprite), but claims no new tile or palette ID at all - and so
// can't repeat the exact tile ID collision Milestone 12's own brute
// once had, by construction.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "sword.h"
#include "sword_pickup.h"

#define SWORD_PICKUP_SPRITE 15 // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, heart_hud.c owns 6-8, pickup.c owns 9, key.c owns 10, brute.c owns 11-14

// Fixed position in room (0,0) - on the same row the player already
// has to walk along to reach the original enemy (y=64), just left of
// its own patrol range (x 96-128), so grabbing it costs no detour.
#define SWORD_PICKUP_X 88
#define SWORD_PICKUP_Y 64

static uint8_t collected;

void sword_pickup_hide(void) {
    move_sprite(SWORD_PICKUP_SPRITE, 0, 0);
}

void sword_pickup_show(void) {
    if (collected) return;
    set_sprite_tile(SWORD_PICKUP_SPRITE, SWORD_TILE_ID);
    set_sprite_prop(SWORD_PICKUP_SPRITE, S_PAL(SWORD_OBJ_PALETTE));
    move_sprite(SWORD_PICKUP_SPRITE, SWORD_PICKUP_X + 8, SWORD_PICKUP_Y + 16);
}

void sword_pickup_init(void) {
    // sword.c's own sword_init() has already loaded this tile/palette
    // data by the time world.c calls this (reset_world()'s own call
    // order) - nothing to load here, just an OAM entry pointing at
    // that existing tile.
    collected = 0;
    sword_pickup_hide(); // world.c's reset_world() shows it explicitly once room state is known
}

uint8_t sword_pickup_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (collected) return 0;

    uint8_t overlap_x = hx < (uint8_t)(SWORD_PICKUP_X + 8) && (uint8_t)(hx + hw) > SWORD_PICKUP_X;
    uint8_t overlap_y = hy < (uint8_t)(SWORD_PICKUP_Y + 8) && (uint8_t)(hy + hh) > SWORD_PICKUP_Y;
    if (!overlap_x || !overlap_y) return 0;

    collected = 1;
    sword_pickup_hide();
    return 1;
}

uint8_t sword_pickup_is_collected(void) { return collected; }

void sword_pickup_load_collected(uint8_t loaded_collected) {
    collected = loaded_collected;
    if (collected) sword_pickup_hide();
}
