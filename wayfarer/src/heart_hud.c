// See heart_hud.h. One hand-authored 8x8 heart-shape tile (a heart
// silhouette isn't formulaic like the diamond/circle shapes every
// other tile asset in this project used, so this one was drawn
// directly as a per-pixel color-index grid, then converted to GB 2bpp
// planar tile bytes by the same one-off local script, verified by
// actually rendering it before being committed here). Two CGB OBJ
// palettes share that one tile - full (bright red) and empty (dim
// gray) - so a heart's filled/empty state is a palette swap, not a
// second bitmap (same technique the sibling prism/ project's own
// board.c uses for its clear-animation flash).

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "heart_hud.h"
#include "player.h"

static const uint8_t heart_tile[16] = {
    0x6C, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0xFE, 0x00,
    0x7C, 0x00, 0x38, 0x00, 0x10, 0x00, 0x00, 0x00,
};

#define HEART_PALETTE_EMPTY 4 // player.c owns 0, sword.c owns 1, enemy.c owns 2, HEART_PALETTE_FULL is 3
static const palette_color_t heart_palettes[2 * 4] = {
    // full: bright red
    RGB(0, 0, 0), RGB(20, 2, 2), RGB(26, 3, 3), RGB(31, 4, 4),
    // empty: dim gray outline
    RGB(0, 0, 0), RGB(6, 6, 7), RGB(9, 9, 10), RGB(12, 12, 13),
};

// Slots 6-8 are the original contiguous 3; slot 9 was already claimed
// by pickup.c before a 4th heart ever existed (Milestone 17's chest),
// so the 4th can't extend the run contiguously - it gets 26 instead,
// the next slot free once boss.c's own 17-25 is accounted for. An
// explicit array, not a formula, since the real layout isn't
// contiguous.
static const uint8_t heart_slots[MAX_HEARTS_CAP] = {6, 7, 8, 26}; // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, pickup.c owns 9, key.c owns 10, brute.c owns 11-14, sword_pickup.c owns 15, shield.c owns 16, boss.c owns 17-25, chest.c owns 27

// Fixed top-left screen position, 12px apart (8px icon + 4px gap).
#define HEART_HUD_X 4
#define HEART_HUD_Y 4
#define HEART_HUD_SPACING 12

void heart_hud_init(void) {
    set_sprite_palette(HEART_PALETTE_FULL, 2, heart_palettes);
    set_sprite_data(HEART_TILE_ID, 1, heart_tile);

    for (uint8_t i = 0; i < MAX_HEARTS_CAP; i++) {
        uint8_t slot = heart_slots[i];
        uint8_t x = (uint8_t)(HEART_HUD_X + i * HEART_HUD_SPACING);
        set_sprite_tile(slot, HEART_TILE_ID);
        move_sprite(slot, x + 8, HEART_HUD_Y + 16);
    }

    heart_hud_update();
}

void heart_hud_update(void) {
    uint8_t hearts = player_get_hearts();
    uint8_t max_hearts = player_get_max_hearts();
    for (uint8_t i = 0; i < MAX_HEARTS_CAP; i++) {
        uint8_t slot = heart_slots[i];
        if (i >= max_hearts) {
            move_sprite(slot, 0, 0);
            continue;
        }
        uint8_t x = (uint8_t)(HEART_HUD_X + i * HEART_HUD_SPACING);
        move_sprite(slot, x + 8, HEART_HUD_Y + 16);
        uint8_t palette = (i < hearts) ? HEART_PALETTE_FULL : HEART_PALETTE_EMPTY;
        set_sprite_prop(slot, S_PAL(palette));
    }
}
