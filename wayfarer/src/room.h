// A single room's tile/palette art plus its own wall layout - the
// visible 20x18-tile screen, a 1-tile-thick wall ring around whichever
// sides have no neighboring room (see world.h), open floor everywhere
// else including straight through an open side's edge. See room.c for
// the tile/palette data and layout.

#ifndef WAYFARER_ROOM_H
#define WAYFARER_ROOM_H

#include <stdint.h>

// The plain "solid color index 0 everywhere" tile - exported so win.c
// can reuse it directly to blank the screen under its own palette
// (the same "one tile, palette swap" trick heart_hud.c's full/empty
// hearts already use) rather than loading a second, identical blank
// tile.
#define FLOOR_TILE_ID 0

// The room's playable interior on a *closed* (walled) side, in pixels -
// the 16x16 player sprite's top-left corner must stay within
// [ROOM_MIN_X, ROOM_MAX_X] x [ROOM_MIN_Y, ROOM_MAX_Y] on that axis to
// keep the whole sprite off the 1-tile (8px) wall. Derived directly
// from the 20x18-tile, 160x144px room: left/top wall is tile column/
// row 0 (pixels 0-7), right wall is column 19 (pixels 152-159, so the
// sprite's right edge must not pass 152), bottom wall is row 17
// (pixels 136-143, so the sprite's bottom edge must not pass 136).
#define ROOM_MIN_X 8
#define ROOM_MAX_X 136
#define ROOM_MIN_Y 8
#define ROOM_MAX_Y 120

// Every room shares this same coordinate system, so one center point
// serves as both the player's initial spawn (player.c) and the
// respawn target after losing all hearts (world.c) - one source of
// truth rather than two independent copies of the same arithmetic.
#define ROOM_CENTER_X ((ROOM_MIN_X + ROOM_MAX_X) / 2)
#define ROOM_CENTER_Y ((ROOM_MIN_Y + ROOM_MAX_Y) / 2)

// The absolute screen bounds a 16x16 sprite's top-left corner can ever
// occupy, regardless of walls - the hard clamp on an *open* side, so a
// uint8_t position can never step past the true edge (which would wrap
// rather than go negative). world.c treats landing exactly on one of
// these as the transition trigger.
#define ABS_MIN_X 0
#define ABS_MAX_X 144
#define ABS_MIN_Y 0
#define ABS_MAX_Y 128

// One-time tile/palette load - call once at startup. Tile *data* is
// the same for every room; only the tile *map* (room_draw()) changes
// between rooms.
void room_init(void);

// Which side, if any, should render as a door texture instead of a
// plain wall when that side is closed (world.c's door_side_for()) -
// purely cosmetic, does not affect collision (room_blocks() below is
// driven entirely by the has_* flags, same as any other closed side).
#define DOOR_NONE 0
#define DOOR_NORTH 1
#define DOOR_SOUTH 2
#define DOOR_EAST 3
#define DOOR_WEST 4

// Builds and draws this room's tile map: a wall on any side whose flag
// is 0, open floor (including straight through the true screen edge)
// on any side whose flag is 1. A corner tile is only floor if *both*
// of its adjoining sides are open - a corner with just one open side
// still shows a wall cap, matching a real doorway/opening rather than
// a floating single-tile gap. Also stores the 4 flags for room_blocks()
// to use. Called once at world init and again on every room
// transition (world.c).
void room_draw(uint8_t has_north, uint8_t has_south, uint8_t has_east, uint8_t has_west, uint8_t door_side);

// Would a 16x16 sprite whose top-left corner is at (px,py) overlap a
// wall, given the current room's open/closed sides (room_draw())? A
// closed side blocks at ROOM_MIN/MAX_*; an open side relaxes that out
// to ABS_MIN/MAX_* (the true screen edge) but no further.
uint8_t room_blocks(uint8_t px, uint8_t py);

#endif
