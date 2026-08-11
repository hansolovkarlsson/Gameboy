// One shield pickup, belonging to room (2,0) only - the last of the
// two rooms Milestone 10 left as empty exploration space (Milestone 12
// already populated the other, (2,1), with the brute). world.c shows/
// hides it on entering/leaving that room and checks collection each
// frame while there, the same shape every other room-bound pickup in
// this project already uses. Once collected, world.c calls
// shield_blocks() at each contact-damage site in place of an
// unconditional hit - a real skill element (classic Zelda-style
// blocking), not a flat damage-immunity upgrade: only a threat on the
// side the player is actively facing is blocked. See shield.c.

#ifndef WAYFARER_SHIELD_H
#define WAYFARER_SHIELD_H

#include <stdint.h>

void shield_init(void);

// Repositions/hides the sprite - call on entering/leaving room (2,0).
// Same "no per-frame update to fall back on" reasoning as every other
// stationary pickup here - world.c must call shield_show() explicitly
// on entry.
void shield_show(void);
void shield_hide(void);

// If not yet collected and the given AABB overlaps the shield's own
// 8x8 AABB, collects it (hides the sprite, stays collected) and
// returns 1; otherwise returns 0 and changes nothing.
uint8_t shield_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh);

// Has the shield been collected yet this session?
uint8_t shield_is_collected(void);

// True only if the shield is collected AND the player - at (px,py) with
// size (pw,ph) and the given facing (player.h's PLAYER_FACING_* values)
// - is currently blocking a contact from a threat at (tx,ty,tw,th).
// Compares box centers to find which side the threat is on (the axis
// with the larger absolute distance - a tie favors horizontal, the
// same "fixed, documented tie-break" style world.c's own room-
// transition checking order already uses) and requires facing to
// point at that same side - not merely "shield equipped." world.c
// calls this at each contact-damage site in place of an unconditional
// hit.
uint8_t shield_blocks(uint8_t px, uint8_t py, uint8_t pw, uint8_t ph,
                       uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th,
                       uint8_t facing);

// Directly overrides the collected state (hiding the sprite if already
// collected) - used once at boot to restore a persisted save
// (world.c/sram.c), bypassing shield_try_collect()'s own AABB check
// entirely since this is a state load, not a gameplay event.
void shield_load_collected(uint8_t collected);

#endif
