// A small 2x2 grid of rooms (room.h) - tracks which room the player is
// currently in and cuts to the adjacent room the instant the player
// steps off an open (neighbor-having) edge of the screen. See world.c.

#ifndef WAYFARER_WORLD_H
#define WAYFARER_WORLD_H

#include <stdint.h>

void world_init(void);
void world_update(uint8_t joy);

#endif
