// See chest.h. A real new 8x8 tile - a gold chest silhouette (drawn
// directly as a per-pixel color-index grid, same "draw directly,
// verify by rendering" approach heart_hud.c's heart/key.c's key/
// shield.c's shield already used, since a chest's own outline isn't
// formulaic like the diamond/circle shapes some other tile assets here
// used). Every hand-drawn tile in this project stores identical low/
// high bitplane bytes per row (confirmed by inspecting heart_tile/
// key_tile/shield_tile directly), giving exactly one solid color plus
// transparent from a single row-bitmask formula rather than 4 real
// colors - this tile follows the same convention.
//
// All 8 CGB OBJ palettes are already claimed (player 0, sword 1,
// enemy 2, heart_hud 3-4, key 5, brute 6, shield 7) - a real hardware
// constraint, not a stylistic one (see boss.c's own comment for the
// bug this caused the first time a module tried claiming a 9th
// anyway). Reuses KEY_PALETTE directly instead - already a gold ramp,
// thematically apt for treasure, and safe: the key (room (1,0)) and
// this chest (room (2,0)) never appear on screen at the same time, the
// same "reuse an existing palette in a room that's never simultaneous"
// reasoning boss.c already established with BRUTE_OBJ_PALETTE.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "chest.h"
#include "key.h"

#define CHEST_TILE_ID 31 // player.c owns 0-11, sword.c owns 12-13, enemy.c owns 14, heart_hud.c owns 15, key.c owns 16, brute.c owns 17-20, shield.c owns 21, boss.c owns 22-30
static const uint8_t chest_tile[16] = {
    // lid (inset top corners)
    0x7E, 0x7E,
    // lid fill
    0xFF, 0xFF,
    // hinge seam - a transparent line separating lid from box
    0x00, 0x00,
    // box top
    0xFF, 0xFF,
    // keyhole notch
    0xE7, 0xE7,
    // box fill
    0xFF, 0xFF,
    0xFF, 0xFF,
    // box bottom (inset corners)
    0x7E, 0x7E,
};

// No set_sprite_palette() call here - KEY_PALETTE's own color data is
// already loaded by key_init(), which reset_world() always calls
// before this (world.c's own call order); reusing the same palette
// index needs it set on this module's own sprite, not reloaded a
// second time.
#define CHEST_PALETTE KEY_PALETTE

#define CHEST_SPRITE 27 // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, heart_hud.c owns 6-8/26, pickup.c owns 9, key.c owns 10, brute.c owns 11-14, sword_pickup.c owns 15, shield.c owns 16, boss.c owns 17-25

// Fixed position in room (2,0), deliberately clear of every script
// that already visits this room: input_script_m12_brute.txt's and
// input_script_m16_boss.txt's own RIGHT-then-DOWN legs sweep the top
// row (y=8) then the right column (x=136) on their way south into
// (2,1)/(2,2); input_script_m14_shield.txt's own DOWN leg sweeps
// straight down the x=72 column. (32,96) intersects none of them.
#define CHEST_X 32
#define CHEST_Y 96

static uint8_t collected;

void chest_hide(void) {
    move_sprite(CHEST_SPRITE, 0, 0);
}

void chest_show(void) {
    if (collected) return;
    set_sprite_tile(CHEST_SPRITE, CHEST_TILE_ID);
    set_sprite_prop(CHEST_SPRITE, S_PAL(CHEST_PALETTE));
    move_sprite(CHEST_SPRITE, CHEST_X + 8, CHEST_Y + 16);
}

void chest_init(void) {
    set_sprite_data(CHEST_TILE_ID, 1, chest_tile);
    collected = 0;
    chest_hide(); // world.c's reset_world() shows it explicitly once room state is known
}

uint8_t chest_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (collected) return 0;

    uint8_t overlap_x = hx < (uint8_t)(CHEST_X + 8) && (uint8_t)(hx + hw) > CHEST_X;
    uint8_t overlap_y = hy < (uint8_t)(CHEST_Y + 8) && (uint8_t)(hy + hh) > CHEST_Y;
    if (!overlap_x || !overlap_y) return 0;

    collected = 1;
    chest_hide();
    return 1;
}

uint8_t chest_is_collected(void) { return collected; }

void chest_load_collected(uint8_t loaded_collected) {
    collected = loaded_collected;
    if (collected) chest_hide();
}
