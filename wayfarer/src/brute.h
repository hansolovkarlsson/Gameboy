// A second, bigger enemy - a 16x16 (vs. the original enemy.c's 8x8)
// blob patrolling vertically in room (2,1), one of the two empty
// rooms Milestone 10 added. Deliberately optional: not required to
// win, purely extra content in the explored space. Takes two hits to
// die (vs. enemy.c's one) - see brute.c for the tile/palette data and
// the post-hit cooldown that makes "two hits" actually mean two
// separate swings, not one swing landing twice.

#ifndef WAYFARER_BRUTE_H
#define WAYFARER_BRUTE_H

#include <stdint.h>

// The brute's own OBJ palette index - exported (the single source of
// truth; brute.c itself uses this same constant rather than a second,
// private copy) so boss.c can deliberately reuse the exact same violet
// ramp: CGB hardware has only 8 OBJ palette slots total (confirmed
// against docs/HARDWARE_REFERENCE.md), and this project's own existing
// modules (player, sword, enemy, heart_hud x2, key, brute, shield)
// already claim all 8 of them - a boss added after that has nothing
// left to claim as its own and must reuse one, not invent a 9th.
#define BRUTE_OBJ_PALETTE 6 // player.c owns 0, sword.c owns 1, enemy.c owns 2, heart_hud.c owns 3-4, key.c owns 5

void brute_init(void);

// Steps the patrol and repositions the sprite, and counts down the
// post-hit cooldown - no-op (and stays hidden) once defeated. Only
// call while the current room is (2,1).
void brute_update(void);

// Moves all 4 quadrant sprites off-screen without changing alive/hp
// state - call when the player leaves room (2,1), so a still-alive
// brute doesn't linger on screen in a different room. Also called by
// brute_init() itself, since room (2,1) is never the boot room (unlike
// enemy.c's room (0,0)), so a freshly-init'd brute must start hidden.
void brute_hide(void);

// If alive and not still in its own post-hit cooldown, and the given
// AABB overlaps the brute's own 16x16 AABB: registers one hit and
// returns 1 (still alive) or 2 (that was the second hit - now
// defeated, sprite hidden). Returns 0 if no hit was registered
// (already dead, still cooling down from a prior hit this swing, or a
// genuine miss) - callers use this 3-way result to choose which sound
// effect to play, unlike enemy.c's plain boolean.
uint8_t brute_try_hit(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Read-only accessors (same shape as enemy.c's own) - world.c needs
// these for its separate player-vs-brute contact-damage AABB check.
uint8_t brute_get_x(void);
uint8_t brute_get_y(void);
uint8_t brute_is_alive(void);

// Directly overrides the alive/hp state (hiding the sprite if loading
// as already-defeated) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing brute_try_hit()'s own AABB check
// entirely since this is a state load, not a gameplay event.
void brute_load_defeated(uint8_t defeated);

#endif
