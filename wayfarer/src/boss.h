// A third, bigger enemy still - a 24x24 blob (vs. the original enemy's
// 8x8 and the brute's own 16x16) patrolling room (2,2), the new dead-
// end room Milestone 16 added south of the brute's own (2,1). Bounces
// independently on *both* axes (every prior enemy here moves on one
// axis only) within a fixed box. Deliberately optional, same as the
// brute: not required to win. Takes three hits to die (vs. the
// brute's two) - see boss.c for the tile/palette data and the same
// post-hit cooldown shape the brute already established, so "three
// hits" actually means three separate swings, not one swing landing
// three times.

#ifndef WAYFARER_BOSS_H
#define WAYFARER_BOSS_H

#include <stdint.h>

void boss_init(void);

// Steps the patrol (both axes) and repositions the sprite, and counts
// down the post-hit cooldown - no-op (and stays hidden) once defeated.
// Only call while the current room is (2,2).
void boss_update(void);

// Moves all 9 quadrant sprites off-screen without changing alive/hp
// state - call when the player leaves room (2,2). Also called by
// boss_init() itself, since room (2,2) is never the boot room, so a
// freshly-init'd boss must start hidden (the same reasoning brute.c's
// own brute_init() already documents).
void boss_hide(void);

// If alive and not still in its own post-hit cooldown, and the given
// AABB overlaps the boss's own 24x24 AABB: registers one hit and
// returns 1 (still alive) or 2 (that was the third hit - now defeated,
// sprite hidden). Returns 0 if no hit was registered.
uint8_t boss_try_hit(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Read-only accessors (same shape as enemy.c's/brute.c's own) -
// world.c needs these for its separate player-vs-boss contact-damage
// AABB check (gated through shield.c's own shield_blocks(), unchanged
// - the boss reuses that mechanism entirely, no new blocking logic).
uint8_t boss_get_x(void);
uint8_t boss_get_y(void);
uint8_t boss_is_alive(void);

// Directly overrides the alive/hp state (hiding the sprite if loading
// as already-defeated) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing boss_try_hit()'s own AABB check entirely
// since this is a state load, not a gameplay event.
void boss_load_defeated(uint8_t defeated);

#endif
