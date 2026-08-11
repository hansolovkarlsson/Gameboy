// A one-shot sword swing - edge-triggered by A, a brief 8x8 blade
// sprite held one tile-width beyond whichever edge of the player the
// current facing points at. See sword.c for the tile/palette data.

#ifndef WAYFARER_SWORD_H
#define WAYFARER_SWORD_H

#include <stdint.h>

// The vertical-blade tile ID and OBJ palette index - exported (the
// single source of truth; sword.c itself uses these same constants
// rather than a second, private copy) so sword_pickup.c can draw the
// exact same art as a static on-ground icon, not a redrawn duplicate.
#define SWORD_TILE_ID 12 // player.c owns sprite tile IDs 0-11
#define SWORD_OBJ_PALETTE 1 // player.c owns 0

void sword_init(void);

// pressed_a: A's edge-triggered (newly-pressed-this-frame) state, not
// held state - one press starts one swing. Recomputes the hitbox from
// the player's *current* position/facing every active frame (the
// player isn't movement-locked during a swing).
void sword_update(uint8_t pressed_a);

// Valid only while sword_is_active() - the swing's current 8x8 hitbox,
// for enemy.c's hit check.
uint8_t sword_is_active(void);
uint8_t sword_get_x(void);
uint8_t sword_get_y(void);

#endif
