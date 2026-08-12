// One treasure chest, belonging to room (2,0) only (the shield's own
// room - the second precedent, after (0,0)'s enemy + sword_pickup, for
// two independent pickups sharing a room). world.c shows/hides it on
// entering/leaving that room and checks collection each frame while
// there, the same shape every other room-bound pickup in this project
// already uses. Collecting it permanently raises the player's own max
// hearts by one (player_increase_max_hearts(), player.h) - real,
// lasting progression, not just a full heal. See chest.c.

#ifndef WAYFARER_CHEST_H
#define WAYFARER_CHEST_H

#include <stdint.h>

void chest_init(void);

// Repositions/hides the sprite - call on entering/leaving room (2,0).
// Same "no per-frame update to fall back on" reasoning as every other
// stationary pickup here - world.c must call chest_show() explicitly
// on entry.
void chest_show(void);
void chest_hide(void);

// If not yet collected and the given AABB overlaps the chest's own 8x8
// AABB, collects it (hides the sprite, stays collected) and returns 1;
// otherwise returns 0 and changes nothing. world.c calls
// player_increase_max_hearts() itself on a successful collect - this
// module only tracks its own collected state, same division of
// responsibility every other pickup here already uses (compare
// pickup.c, which doesn't call player_heal_full() itself either).
uint8_t chest_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Has the chest been collected yet this session?
uint8_t chest_is_collected(void);

// Directly overrides the collected state (hiding the sprite if already
// collected) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing chest_try_collect()'s own AABB check
// entirely since this is a state load, not a gameplay event. world.c
// is responsible for also calling player_increase_max_hearts() itself
// when a loaded save says the chest is already collected - this
// function only restores the sprite/collected-flag state.
void chest_load_collected(uint8_t collected);

#endif
