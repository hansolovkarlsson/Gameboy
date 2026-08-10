// See room.h. Three 8x8 BG tiles - floor (a plain, empty tile: the
// room background tone alone, color index 0 everywhere, same "blank
// tile shows the palette's own background color" convention Prism's
// gems.c/board.c already use), wall (a simple beveled stone block: a
// dark top/left edge, a lighter bottom/right edge, solid fill
// between), and door (a warm gold fill with a dark vertical center
// seam, suggesting a pair of doors - Milestone 5's locked door) - not
// hand-drawn pixel by pixel, computed from those rules and converted
// to GB 2bpp planar tile bytes by a one-off local script, same
// discipline as every tile asset in the sibling prism/ project),
// verified by actually rendering this room through this project's own
// emulator before being committed here.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "room.h"

#define FLOOR_TILE_ID 0
#define WALL_TILE_ID 1
#define DOOR_TILE_ID 2
#define ROOM_TILE_COUNT 3

static const uint8_t room_tiles[ROOM_TILE_COUNT * 16] = {
    // floor: solid color index 0 (the room's background tone)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // wall: dark bevel (color 1) on top/left, light bevel (color 2) on
    // bottom/right, solid fill (color 3) between
    0xFF, 0x00, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F,
    0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0x80, 0x7F,
    // door: gold fill (color 3), dark center seam (color 1)
    0xFF, 0xE7, 0xFF, 0xE7, 0xFF, 0xE7, 0xFF, 0xE7,
    0xFF, 0xE7, 0xFF, 0xE7, 0xFF, 0xE7, 0xFF, 0xE7,
};

// One CGB BG palette: color 0 a neutral tan floor tone, color 1 a dark
// stone shadow, color 2 a lighter stone highlight, color 3 a mid-tone
// stone fill - only the wall tile uses 1-3, only the floor tile uses 0.
#define ROOM_PALETTE 0
static const palette_color_t room_palette[4] = {
    RGB(18, 16, 10), RGB(8, 8, 10), RGB(20, 20, 22), RGB(14, 14, 16),
};

// A second BG palette, only for the door tile: color 1 a dark wood
// seam, color 3 a warm gold fill - distinct enough from the plain
// stone wall to read as "something that opens," not just more wall.
#define DOOR_PALETTE 1
static const palette_color_t door_palette[4] = {
    RGB(18, 16, 10), RGB(10, 6, 2), RGB(20, 20, 22), RGB(24, 18, 4),
};

#define ROOM_TILE_W 20
#define ROOM_TILE_H 18

// Set by room_draw(), read by room_blocks() - which sides of the
// *current* room are open (a neighboring room exists there, world.c)
// versus walled.
static uint8_t room_has_north;
static uint8_t room_has_south;
static uint8_t room_has_east;
static uint8_t room_has_west;

void room_init(void) {
    set_bkg_palette(ROOM_PALETTE, 1, room_palette);
    set_bkg_palette(DOOR_PALETTE, 1, door_palette);
    set_bkg_data(FLOOR_TILE_ID, ROOM_TILE_COUNT, room_tiles);
}

void room_draw(uint8_t has_north, uint8_t has_south, uint8_t has_east, uint8_t has_west, uint8_t door_side) {
    room_has_north = has_north;
    room_has_south = has_south;
    room_has_east = has_east;
    room_has_west = has_west;

    uint8_t map_tiles[ROOM_TILE_W * ROOM_TILE_H];
    uint8_t map_attrs[ROOM_TILE_W * ROOM_TILE_H];
    for (uint8_t y = 0; y < ROOM_TILE_H; y++) {
        for (uint8_t x = 0; x < ROOM_TILE_W; x++) {
            uint8_t on_west = x == 0 && !has_west;
            uint8_t on_east = x == ROOM_TILE_W - 1 && !has_east;
            uint8_t on_north = y == 0 && !has_north;
            uint8_t on_south = y == ROOM_TILE_H - 1 && !has_south;
            uint8_t is_wall = on_west || on_east || on_north || on_south;

            uint8_t is_door = (door_side == DOOR_WEST && on_west) ||
                               (door_side == DOOR_EAST && on_east) ||
                               (door_side == DOOR_NORTH && on_north) ||
                               (door_side == DOOR_SOUTH && on_south);

            uint8_t tile_id = FLOOR_TILE_ID;
            uint8_t palette = ROOM_PALETTE;
            if (is_wall) {
                tile_id = is_door ? DOOR_TILE_ID : WALL_TILE_ID;
                palette = is_door ? DOOR_PALETTE : ROOM_PALETTE;
            }
            map_tiles[y * ROOM_TILE_W + x] = tile_id;
            map_attrs[y * ROOM_TILE_W + x] = palette;
        }
    }
    set_bkg_tiles(0, 0, ROOM_TILE_W, ROOM_TILE_H, map_tiles);
    set_bkg_attributes(0, 0, ROOM_TILE_W, ROOM_TILE_H, map_attrs);
}

uint8_t room_blocks(uint8_t px, uint8_t py) {
    uint8_t min_x = room_has_west ? ABS_MIN_X : ROOM_MIN_X;
    uint8_t max_x = room_has_east ? ABS_MAX_X : ROOM_MAX_X;
    uint8_t min_y = room_has_north ? ABS_MIN_Y : ROOM_MIN_Y;
    uint8_t max_y = room_has_south ? ABS_MAX_Y : ROOM_MAX_Y;
    return (px < min_x) || (px > max_x) || (py < min_y) || (py > max_y);
}
