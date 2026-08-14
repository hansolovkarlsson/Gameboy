// See goal.c. A single stationary flag marker on tier 3 (the top
// platform) - reaching it is this game's win condition.

#ifndef ASCENT_GOAL_H
#define ASCENT_GOAL_H

#include <stdint.h>

void goal_init(void);

// AABB overlap test against the player's 16x16 box (top-left pixel).
// Takes the player's position as parameters rather than including
// player.h, the same decoupling barrel.h's own collision check uses.
uint8_t goal_check_reached(uint8_t player_x, uint8_t player_y);

#endif
