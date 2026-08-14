// See player.c. A single 16x16 (2x2-tile) side-profile sprite - drawn
// facing right, mirrored via S_FLIPX for left - that falls under
// gravity, stands on stage.h's floor/ladder-top tiles, climbs its
// ladder tiles, and (Milestone 2) jumps a fixed arc with air control.

#ifndef ASCENT_PLAYER_H
#define ASCENT_PLAYER_H

#include <stdint.h>

void player_init(void);
void player_update(uint8_t joy);

// The player's current top-left pixel - barrel.c's own collision check
// needs these to build the player's AABB without player.c depending on
// barrel.c (main.c wires the two together instead).
uint8_t player_get_x(void);
uint8_t player_get_y(void);

// True while a jump is in progress (rise or fall) - score.c's own
// barrel-jump bonus (main.c wires it in) only counts a barrel passed
// underneath while actually airborne, not one merely brushed past
// while grounded (which barrel_check_hit()'s own full AABB already
// turns into a respawn instead).
uint8_t player_is_jumping(void);

// True for a brief window after a respawn - main.c should ignore any
// barrel_check_hit() result while this is true. Needed because
// SPAWN_X sits inside the ground tier's own barrel travel range: a
// barrel still overlapping the respawn point would otherwise
// re-trigger a hit on literally the next frame, burning through every
// one of lives.c's own lives in the span of one barrel's pass rather
// than across genuinely separate encounters.
uint8_t player_is_invincible(void);

// Snaps the player back to the spawn point (the same 8px-above-rest
// drop-in player_init() already uses, so the existing gravity code
// animates the "drop back in" for free), clears any in-progress jump,
// and arms a brief invincibility window (see player_is_invincible()) -
// called by main.c when barrel.c reports a hit and lives remain.
void player_respawn(void);

#endif
