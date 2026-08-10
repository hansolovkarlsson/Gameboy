// A single, static, fully-bordered room - the whole visible 20x18-tile
// screen, a 1-tile-thick wall ring around the outside and open floor
// everywhere inside. See room.c for the tile/palette data and layout.

#ifndef WAYFARER_ROOM_H
#define WAYFARER_ROOM_H

#include <stdint.h>

// The room's playable interior, in pixels - the 16x16 player sprite's
// top-left corner must stay within [ROOM_MIN_X, ROOM_MAX_X] x
// [ROOM_MIN_Y, ROOM_MAX_Y] to keep the whole sprite off the 1-tile
// (8px) wall ring. Derived directly from the 20x18-tile, 160x144px
// room: left/top wall is tile column/row 0 (pixels 0-7), right wall is
// column 19 (pixels 152-159, so the sprite's right edge must not pass
// 152), bottom wall is row 17 (pixels 136-143, so the sprite's bottom
// edge must not pass 136).
#define ROOM_MIN_X 8
#define ROOM_MAX_X 136
#define ROOM_MIN_Y 8
#define ROOM_MAX_Y 120

void room_init(void);

// Would a 16x16 sprite whose top-left corner is at (px,py) overlap the
// wall ring? Only the outer ring exists this milestone, so a plain
// bounds check is the whole story - no general tile-by-tile lookup
// needed yet.
uint8_t room_blocks(uint8_t px, uint8_t py);

#endif
