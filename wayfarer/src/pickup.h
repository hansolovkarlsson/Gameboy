// One heart pickup, belonging to room (1,1) only - world.c shows/hides
// it on entering/leaving that room and checks collection each frame
// while there. See pickup.c.

#ifndef WAYFARER_PICKUP_H
#define WAYFARER_PICKUP_H

#include <stdint.h>

void pickup_init(void);

// Repositions/hides the sprite - call on entering/leaving room (1,1).
// Unlike enemy.c's patrol, a stationary pickup has no per-frame update
// to fall back on for re-showing itself, so world.c must call
// pickup_show() explicitly on entry.
void pickup_show(void);
void pickup_hide(void);

// If alive and the given AABB overlaps the pickup's own 8x8 AABB,
// collects it (hides the sprite, stays collected) and returns 1;
// otherwise returns 0 and changes nothing.
uint8_t pickup_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Directly overrides the collected state (hiding the sprite if already
// collected) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing pickup_try_collect()'s own AABB check
// entirely since this is a state load, not a gameplay event.
void pickup_load_collected(uint8_t collected);

#endif
