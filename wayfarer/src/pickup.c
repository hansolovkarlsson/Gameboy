// See pickup.h. Reuses heart_hud.h's own heart tile and "full" (bright
// red) palette directly - the exact same art as a full HUD heart, just
// a different sprite slot/position, so no duplicated tile data.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "heart_hud.h"
#include "pickup.h"
#include "room.h"

#define PICKUP_SPRITE 9 // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, heart_hud.c owns 6-8

static uint8_t alive;

void pickup_hide(void) {
    move_sprite(PICKUP_SPRITE, 0, 0);
}

void pickup_show(void) {
    if (!alive) return;
    set_sprite_tile(PICKUP_SPRITE, HEART_TILE_ID);
    set_sprite_prop(PICKUP_SPRITE, S_PAL(HEART_PALETTE_FULL));
    move_sprite(PICKUP_SPRITE, ROOM_CENTER_X + 8, ROOM_CENTER_Y + 16);
}

void pickup_init(void) {
    alive = 1;
    pickup_hide(); // the game starts in room (0,0), not this pickup's room (1,1)
}

uint8_t pickup_try_collect(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (!alive) return 0;

    uint8_t overlap_x = hx < (uint8_t)(ROOM_CENTER_X + 8) && (uint8_t)(hx + hw) > ROOM_CENTER_X;
    uint8_t overlap_y = hy < (uint8_t)(ROOM_CENTER_Y + 8) && (uint8_t)(hy + hh) > ROOM_CENTER_Y;
    if (!overlap_x || !overlap_y) return 0;

    alive = 0;
    pickup_hide();
    return 1;
}
