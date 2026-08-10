// One key pickup, belonging to room (1,0) only - world.c shows/hides
// it on entering/leaving that room and checks collection each frame
// while there. Once collected it unlocks the locked door between rooms
// (1,1) and (0,1) (world.c's key_is_collected() check). See key.c.

#ifndef WAYFARER_KEY_H
#define WAYFARER_KEY_H

#include <stdint.h>

void key_init(void);

// Repositions/hides the sprite - call on entering/leaving room (1,0).
// Same "no per-frame update to fall back on" reasoning as pickup.c's
// own show()/hide() - world.c must call key_show() explicitly on entry.
void key_show(void);
void key_hide(void);

// If not yet collected and the given AABB overlaps the key's own 8x8
// AABB, collects it (hides the sprite, stays collected) and returns 1;
// otherwise returns 0 and changes nothing.
uint8_t key_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Has the key been collected yet this session? world.c's locked-door
// sides read this directly.
uint8_t key_is_collected(void);

#endif
