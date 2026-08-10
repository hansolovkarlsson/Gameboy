// Score/moves HUD for Prism (Milestone 5,
// docs/GAMEBOY_ROADMAP.md's Phase 10 entry). No GBDK console/font
// system (gbdk/console.h, gbdk/font.h) - its default font's tile-ID
// placement isn't something to assume without real risk of colliding
// with gems.c's own tile IDs 0-4 in the same VRAM bank. A small custom
// digit tileset instead, same "generate programmatically, verify
// visually" approach already used for the gem/cursor/marker tiles.

#ifndef PRISM_HUD_H
#define PRISM_HUD_H

#include <stdint.h>

// Ten 8x8 digit tiles (0-9), extending gems.c's own tile ID range
// (0-4) rather than colliding with it.
#define HUD_DIGIT_TILE_BASE 5
#define HUD_DIGIT_COUNT 10

// The raw digit tile bytes (16 bytes/tile), exported so title.c can
// load the exact same art at its own, different tile-ID range (the
// title screen and the in-game HUD never render at the same time, but
// using different IDs keeps things simple and avoids the exact kind of
// shared-tile-ID surprise this project has already hit twice - see
// Milestone 2's uninitialized tile map and Milestone 4's S_PALETTE/
// S_PAL mixup).
extern const uint8_t hud_digit_tiles[HUD_DIGIT_COUNT * 16];

// Loads the digit tileset. Call once at startup, after board_init()
// (which already blanks the entire background tile map, including
// row 0 - the HUD's own row - via fill_screen_blank(), board.c).
// Draws nothing itself - call hud_set_score()/hud_set_moves() with the
// real starting values right after.
void hud_init(void);

// Redraws only the score field (4 digits, left side of row 0). Call
// only when the value actually changes, not every frame.
void hud_set_score(uint16_t score);

// Redraws only the moves-remaining field (2 digits, right side of
// row 0). Call only when the value actually changes, not every frame.
void hud_set_moves(uint8_t moves);

#endif
