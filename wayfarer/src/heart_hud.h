// A 3-heart, sprite-based HUD (not BG tiles - deliberately avoids
// touching the already-locked-in room/wall geometry from Milestones
// 1-2 at all; sprites simply draw on top of whatever room tile is
// underneath, standard HUD-overlay treatment). See heart_hud.c for the
// tile/palette data. Also exports the heart tile ID and "full" palette
// so pickup.c can reuse the exact same art for the on-floor pickup
// with zero duplication.

#ifndef WAYFARER_HEART_HUD_H
#define WAYFARER_HEART_HUD_H

#include <stdint.h>

#define HEART_TILE_ID 15 // player.c owns 0-11, sword.c owns 12-13, enemy.c owns 14
#define HEART_PALETTE_FULL 3 // player.c owns 0, sword.c owns 1, enemy.c owns 2

void heart_hud_init(void);

// Reads player_get_hearts() and updates each of the 3 heart sprites'
// OBJ palette (full vs. empty) accordingly - the tile ID never
// changes, only which of the two palettes is selected (same "one
// tile, palette-swapped" technique the sibling prism/ project's own
// board.c clear-animation flash uses). Safe to call unconditionally
// every frame regardless of room.
void heart_hud_update(void);

#endif
