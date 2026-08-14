// See player.c. A single 16x16 (2x2-tile) side-profile sprite - drawn
// facing right, mirrored via S_FLIPX for left - that falls under
// gravity, stands on stage.h's floor/ladder-top tiles, and climbs its
// ladder tiles. No jump yet (Milestone 1 - see docs/GAMEBOY_ROADMAP.md).

#ifndef ASCENT_PLAYER_H
#define ASCENT_PLAYER_H

#include <stdint.h>

void player_init(void);
void player_update(uint8_t joy);

#endif
