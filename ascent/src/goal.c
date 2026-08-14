// See goal.h. A small flag-on-a-pole 8x8 sprite, fixed on tier 3 past
// column 4 (the one ladder barrel.c's own barrels ever use on that
// tier - they always descend there, never roll further left, so this
// spot is permanently out of barrel traffic; confirmed against
// stage.c's real tile map, not assumed). Never moves, never hides -
// simpler than chest.c/shield.c's own show/hide pickups, since there's
// nothing to collect, only reach.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "goal.h"

#define GOAL_TILE_ID 5 // player.c owns OBJ tile ids 0-3, barrel.c owns 4

// Flag silhouette: a pole (column 1, one pixel wide) with a triangular
// flag (columns 2-6, widest at row 3) - same row-bitmask 2-color-
// plus-transparent convention every hand-drawn tile in this project
// already relies on (color 0 hardware-transparent, same as every
// other sprite here).
static const uint8_t goal_tile[16] = {
    0x40, 0x00, // pole only
    0x40, 0x38, // flag starts
    0x40, 0x3C,
    0x40, 0x3E, // flag widest
    0x40, 0x38,
    0x40, 0x00, // pole only
    0x40, 0x00,
    0x40, 0x00,
};

// One CGB OBJ palette: color 0 unused (hardware-transparent), color 1
// a dark wood pole (same tone as stage.c's own girder edge), color 2
// a bright celebratory gold flag, color 3 unused. Palettes 0/1 belong
// to player.c/barrel.c.
#define GOAL_PALETTE 2
static const palette_color_t goal_palette[4] = {
    RGB(0, 0, 0), RGB(10, 6, 2), RGB(28, 22, 4), RGB(0, 0, 0),
};

#define GOAL_SPRITE 6 // player.c owns 0-3, barrel.c owns 4-5

// Tier 3 is stage.c's own row 5 - see stage.c's tile map. Column 4
// (px 32-39) is that tier's only ladder; the goal sits well to its
// left, past the point any barrel ever reaches on this tier.
#define TIER3_TILE_ROW 5
#define GOAL_X 8
#define GOAL_Y (TIER3_TILE_ROW * 8 - 8)

void goal_init(void) {
    set_sprite_palette(GOAL_PALETTE, 1, goal_palette);
    set_sprite_data(GOAL_TILE_ID, 1, goal_tile);
    set_sprite_tile(GOAL_SPRITE, GOAL_TILE_ID);
    set_sprite_prop(GOAL_SPRITE, GOAL_PALETTE);
    move_sprite(GOAL_SPRITE, GOAL_X + 8, GOAL_Y + 16);
}

uint8_t goal_check_reached(uint8_t player_x, uint8_t player_y) {
    return player_x < GOAL_X + 8 && player_x + 16 > GOAL_X &&
           player_y < GOAL_Y + 8 && player_y + 16 > GOAL_Y;
}
