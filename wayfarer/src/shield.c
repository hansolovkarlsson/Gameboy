// See shield.h. A real new 8x8 tile - a rounded-top, pointed-bottom
// heater-shield silhouette (drawn directly as a per-pixel color-index
// grid, same "draw directly, verify by rendering" approach heart_hud.c's
// heart and key.c's key already used, since a shield's own outline
// isn't formulaic like the diamond/circle shapes some other tile
// assets here used), a blue/silver metallic palette distinct from the
// sword's own plain silver.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "player.h"
#include "shield.h"

#define SHIELD_TILE_ID 21 // player.c owns 0-11, sword.c owns 12-13, enemy.c owns 14, heart_hud.c owns 15, key.c owns 16, brute.c owns 17-20
static const uint8_t shield_tile[16] = {
    0x7E, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0x7E, 0x7E, 0x3C, 0x3C, 0x18, 0x18,
};

#define SHIELD_PALETTE 7 // player.c owns 0, sword.c owns 1, enemy.c owns 2, heart_hud.c owns 3-4, key.c owns 5, brute.c owns 6
static const palette_color_t shield_palette[4] = {
    RGB(0, 0, 0), RGB(4, 10, 20), RGB(10, 18, 28), RGB(18, 26, 31),
};

#define SHIELD_SPRITE 16 // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, heart_hud.c owns 6-8, pickup.c owns 9, key.c owns 10, brute.c owns 11-14, sword_pickup.c owns 15

// Fixed position in room (2,0) - deliberately off the y=64 row
// input_script_m12_brute.txt's own script sweeps across this entire
// room on its way to (2,1), so that script's own timing/contact-damage
// exposure stays completely untouched by this pickup's presence.
#define SHIELD_X 72
#define SHIELD_Y 24

static uint8_t collected;

void shield_hide(void) {
    move_sprite(SHIELD_SPRITE, 0, 0);
}

void shield_show(void) {
    if (collected) return;
    set_sprite_tile(SHIELD_SPRITE, SHIELD_TILE_ID);
    set_sprite_prop(SHIELD_SPRITE, S_PAL(SHIELD_PALETTE));
    move_sprite(SHIELD_SPRITE, SHIELD_X + 8, SHIELD_Y + 16);
}

void shield_init(void) {
    set_sprite_palette(SHIELD_PALETTE, 1, shield_palette);
    set_sprite_data(SHIELD_TILE_ID, 1, shield_tile);
    collected = 0;
    shield_hide(); // world.c's reset_world() shows it explicitly once room state is known
}

uint8_t shield_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (collected) return 0;

    uint8_t overlap_x = hx < (uint8_t)(SHIELD_X + 8) && (uint8_t)(hx + hw) > SHIELD_X;
    uint8_t overlap_y = hy < (uint8_t)(SHIELD_Y + 8) && (uint8_t)(hy + hh) > SHIELD_Y;
    if (!overlap_x || !overlap_y) return 0;

    collected = 1;
    shield_hide();
    return 1;
}

uint8_t shield_is_collected(void) { return collected; }

// SDCC's optimizer flags this function's body with "warning 110:
// conditional flow changed by optimizer" at the same 4 lines
// regardless of how the logic below is restructured (tried both a
// signed-int16_t and this unsigned-only rewrite) - a known SDCC quirk,
// not unique to this codebase, and not a real bug: confirmed directly
// against the built ROM with 10 hardcoded geometry cases (all 4
// directions plus the horizontal-wins-tie case, 2 facings each)
// emitted via serial and diffed against hand-computed expected
// results, all matching exactly, before this comment was written.
uint8_t shield_blocks(uint8_t px, uint8_t py, uint8_t pw, uint8_t ph,
                       uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th,
                       uint8_t facing) {
    if (!collected) return 0;

    uint8_t pcx = (uint8_t)(px + pw / 2);
    uint8_t pcy = (uint8_t)(py + ph / 2);
    uint8_t tcx = (uint8_t)(tx + tw / 2);
    uint8_t tcy = (uint8_t)(ty + th / 2);

    // Unsigned throughout, matching every other coordinate computation
    // in this codebase (no other file needs signed arithmetic) - the
    // direction (which side the threat is on) and the distance
    // (how far) are computed as two separate plain comparisons rather
    // than a single signed subtraction.
    uint8_t threat_right = tcx > pcx;
    uint8_t threat_below = tcy > pcy;
    uint8_t dist_x;
    if (tcx > pcx) { dist_x = (uint8_t)(tcx - pcx); } else { dist_x = (uint8_t)(pcx - tcx); }
    uint8_t dist_y;
    if (tcy > pcy) { dist_y = (uint8_t)(tcy - pcy); } else { dist_y = (uint8_t)(pcy - tcy); }

    // Dominant axis by distance - a tie favors horizontal, the same
    // "fixed, documented tie-break" style world.c's own room-
    // transition checking order already uses.
    if (dist_x >= dist_y) {
        if (threat_right) { return facing == PLAYER_FACING_RIGHT; }
        return facing == PLAYER_FACING_LEFT;
    }
    if (threat_below) { return facing == PLAYER_FACING_DOWN; }
    return facing == PLAYER_FACING_UP;
}

void shield_load_collected(uint8_t loaded_collected) {
    collected = loaded_collected;
    if (collected) shield_hide();
}
