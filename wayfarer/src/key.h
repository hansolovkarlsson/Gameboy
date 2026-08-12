// One key pickup, belonging to room (1,0) only - world.c shows/hides
// it on entering/leaving that room and checks collection each frame
// while there. Once collected it unlocks the locked door between rooms
// (1,1) and (0,1) (world.c's key_is_collected() check). See key.c.

#ifndef WAYFARER_KEY_H
#define WAYFARER_KEY_H

#include <stdint.h>

// The key's own OBJ palette index - exported (the single source of
// truth; key.c itself uses this same constant rather than a second,
// private copy) so chest.c can deliberately reuse the exact same gold
// ramp: all 8 CGB OBJ palette slots are already claimed by this
// project's existing modules, so the Milestone 17 chest - thematically
// gold anyway - reuses this one rather than inventing a 9th, the same
// "reuse an existing palette, document why" move boss.c already made
// with brute.h's own BRUTE_OBJ_PALETTE.
#define KEY_PALETTE 5 // player.c owns 0, sword.c owns 1, enemy.c owns 2, heart_hud.c owns 3-4

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

// Directly overrides the collected state (hiding the sprite if already
// collected) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing key_try_collect()'s own AABB check
// entirely since this is a state load, not a gameplay event.
void key_load_collected(uint8_t collected);

#endif
