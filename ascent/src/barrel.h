// See barrel.c. Up to MAX_BARRELS 8x8 barrels, periodically spawned on
// the top platform, that roll along a tier and descend through
// whichever ladder column they cross - retracing the player's own
// stage.c climb route in reverse.

#ifndef ASCENT_BARREL_H
#define ASCENT_BARREL_H

#include <stdint.h>

void barrel_init(void);
void barrel_update(void);

// AABB overlap test against every active barrel's 8x8 box, given the
// player's own 16x16 box (top-left pixel). Takes the player's position
// as parameters rather than including player.h, so barrel.c stays
// decoupled from player.c - main.c wires the two together.
uint8_t barrel_check_hit(uint8_t player_x, uint8_t player_y);

// Points earned this call for barrels just cleared by jumping - main.c
// only calls this while player_is_jumping() is true, and each barrel
// only ever pays out once (a per-barrel "already scored" flag, cleared
// only when that slot spawns a fresh barrel). Horizontal overlap only,
// gated to barrels near the player's own vertical position (same
// reasoning as barrel_check_hit's own AABB, just without requiring
// full vertical overlap - the point is "passed over", not "collided
// with") so a barrel on an unrelated tier can never match.
uint16_t barrel_check_jump_score(uint8_t player_x, uint8_t player_y);

#endif
