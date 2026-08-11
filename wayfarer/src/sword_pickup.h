// One sword pickup, belonging to room (0,0) only - the same room the
// original enemy patrols. world.c shows/hides it on entering/leaving
// that room and checks collection each frame while there. Until
// collected, world.c never calls sword_update() at all, so pressing A
// does nothing - matches this project's own "always fully resolve, no
// half-finished states" style: no half-armed state, no swing without a
// sword. See sword_pickup.c.

#ifndef WAYFARER_SWORD_PICKUP_H
#define WAYFARER_SWORD_PICKUP_H

#include <stdint.h>

void sword_pickup_init(void);

// Repositions/hides the sprite - call on entering/leaving room (0,0),
// including the very first frame of a fresh boot (room (0,0) is also
// the boot room). Same "no per-frame update to fall back on" reasoning
// as pickup.c's/key.c's own show()/hide() - world.c must call
// sword_pickup_show() explicitly on entry.
void sword_pickup_show(void);
void sword_pickup_hide(void);

// If not yet collected and the given AABB overlaps the pickup's own
// 8x8 AABB, collects it (hides the sprite, stays collected) and
// returns 1; otherwise returns 0 and changes nothing.
uint8_t sword_pickup_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Has the sword been collected yet this session? world.c reads this
// directly to gate whether A does anything at all.
uint8_t sword_pickup_is_collected(void);

// Directly overrides the collected state (hiding the sprite if already
// collected) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing sword_pickup_try_collect()'s own AABB
// check entirely since this is a state load, not a gameplay event.
void sword_pickup_load_collected(uint8_t collected);

#endif
