// A sprite-based HUD (not BG tiles - deliberately avoids touching the
// already-locked-in room/wall geometry from Milestones 1-2 at all;
// sprites simply draw on top of whatever room tile is underneath,
// standard HUD-overlay treatment), sized for up to player.h's own
// MAX_HEARTS_CAP hearts (3 by default, one more once the Milestone 17
// treasure chest is collected) rather than a fixed 3 - any slot beyond
// the player's current real max simply stays hidden. See heart_hud.c
// for the tile/palette data. Also exports the heart tile ID and "full"
// palette so pickup.c can reuse the exact same art for the on-floor
// pickup with zero duplication.

#ifndef WAYFARER_HEART_HUD_H
#define WAYFARER_HEART_HUD_H

#include <stdint.h>

#include "player.h"

#define HEART_TILE_ID 15 // player.c owns 0-11, sword.c owns 12-13, enemy.c owns 14
#define HEART_PALETTE_FULL 3 // player.c owns 0, sword.c owns 1, enemy.c owns 2

void heart_hud_init(void);

// Reads player_get_hearts()/player_get_max_hearts() and updates each
// heart sprite up to the current max: OBJ palette (full vs. empty) for
// an unlocked slot, hidden entirely for a slot beyond the current max
// - the tile ID never changes, only which of the two palettes is
// selected (same "one tile, palette-swapped" technique the sibling
// prism/ project's own board.c clear-animation flash uses). Safe to
// call unconditionally every frame regardless of room, and regardless
// of whether max_hearts has grown since the last call.
void heart_hud_update(void);

#endif
