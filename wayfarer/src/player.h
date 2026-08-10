// A 16x16 directional player sprite - continuous (not grid-snapped)
// movement, held-direction-responsive, collision-checked against
// room.h's wall bounds. See player.c for the tile/palette data.

#ifndef WAYFARER_PLAYER_H
#define WAYFARER_PLAYER_H

#include <stdint.h>

void player_init(void);

// Reads the currently-held D-pad state (not edge-triggered - this is
// what makes movement continuous/real-time rather than a menu cursor's
// single-step-per-press) and moves/repositions the sprite accordingly.
void player_update(uint8_t joy);

// Read-only position accessors (same shape as the sibling prism/
// project's cursor.c cursor_get_x()/get_y()) - world.c needs these to
// detect a room-edge transition.
uint8_t player_get_x(void);
uint8_t player_get_y(void);

// Matches the internal facing_t enum's own values exactly - sword.c
// needs to know which way to swing.
#define PLAYER_FACING_DOWN 0
#define PLAYER_FACING_UP 1
#define PLAYER_FACING_LEFT 2
#define PLAYER_FACING_RIGHT 3
uint8_t player_get_facing(void);

// Repositions the player immediately (redraws the sprite the same
// frame, not one frame stale) without going through player_update()'s
// own movement/collision logic - used by world.c right after a room
// transition to place the player just inside the new room's opposite
// edge.
void player_set_position(uint8_t x, uint8_t y);

// Current heart count (0..MAX_HEARTS, see player.c).
uint8_t player_get_hearts(void);

// Subtracts amount from hearts (clamped at 0) and starts a brief
// invincibility window - a no-op entirely if still invincible from a
// prior hit, so a multi-frame graze against one hazard only ever costs
// one heart.
void player_damage(uint8_t amount);

// Resets hearts to the maximum - shared by both the heart pickup and
// the on-death respawn path (world.c).
void player_heal_full(void);

#endif
