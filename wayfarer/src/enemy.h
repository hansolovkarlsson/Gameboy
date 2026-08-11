// One simple patrolling enemy, belonging to room (0,0) only - world.c
// gates when enemy_update()/enemy_try_hit() are called and hides it
// when the player leaves that room. See enemy.c for the tile/palette
// data and patrol range.

#ifndef WAYFARER_ENEMY_H
#define WAYFARER_ENEMY_H

#include <stdint.h>

void enemy_init(void);

// Steps the patrol and repositions the sprite - no-op (and stays
// hidden) once defeated. Only call while the current room is (0,0).
void enemy_update(void);

// Moves the sprite off-screen without changing "alive" state - call
// when the player leaves room (0,0), so a still-alive enemy doesn't
// linger on screen in a different room.
void enemy_hide(void);

// If alive and the given AABB overlaps the enemy's own 8x8 AABB,
// defeats it (hides the sprite, stays defeated) and returns 1;
// otherwise returns 0 and changes nothing.
uint8_t enemy_try_hit(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Read-only accessors (same shape as player.c's own) - world.c needs
// these for its separate player-vs-enemy contact-damage AABB check
// (enemy_try_hit() is specifically the sword-vs-enemy one, and also
// mutates state, so it isn't reused for this read-only check).
uint8_t enemy_get_x(void);
uint8_t enemy_get_y(void);
uint8_t enemy_is_alive(void);

// Directly overrides the alive state (hiding the sprite if loading as
// already-defeated) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing enemy_try_hit()'s own AABB check
// entirely since this is a state load, not a gameplay event.
void enemy_load_defeated(uint8_t defeated);

#endif
