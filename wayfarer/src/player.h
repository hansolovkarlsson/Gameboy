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

#endif
