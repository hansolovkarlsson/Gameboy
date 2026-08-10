// See world.h. A 2x2 grid (GRID_W x GRID_H) - the smallest grid that
// genuinely exercises both axes of transition (east/west and north/
// south) in one milestone. Every room in a 2x2 grid is a corner, so
// every room has exactly 2 open sides and 2 walled sides - a real,
// easy-to-verify visual fingerprint per room (room (0,0): east+south
// open; (1,0): west+south open; (0,1): east+north open; (1,1):
// west+north open), not an artificial marker added just for testing.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "player.h"
#include "room.h"
#include "world.h"

#define GRID_W 2
#define GRID_H 2

static uint8_t room_x;
static uint8_t room_y;

static void draw_current_room(void) {
    uint8_t has_west = room_x > 0;
    uint8_t has_east = room_x < GRID_W - 1;
    uint8_t has_north = room_y > 0;
    uint8_t has_south = room_y < GRID_H - 1;
    room_draw(has_north, has_south, has_east, has_west);
}

void world_init(void) {
    room_x = 0;
    room_y = 0;
    room_init(); // one-time tile/palette load
    draw_current_room();
    player_init();
}

void world_update(uint8_t joy) {
    player_update(joy);

    uint8_t has_west = room_x > 0;
    uint8_t has_east = room_x < GRID_W - 1;
    uint8_t has_north = room_y > 0;
    uint8_t has_south = room_y < GRID_H - 1;

    uint8_t px = player_get_x();
    uint8_t py = player_get_y();

    uint8_t new_room_x = room_x;
    uint8_t new_room_y = room_y;
    uint8_t entry_x = px;
    uint8_t entry_y = py;
    uint8_t transitioning = 0;

    // Fixed west/east/north/south checking order - simplest
    // deterministic tie-break for the rare exact-corner case (both an
    // open vertical and open horizontal edge reached the same frame
    // via diagonal input), same "fixed priority, documented" choice
    // player.c's own diagonal-facing logic already makes.
    if (px == ABS_MIN_X && has_west) {
        new_room_x = (uint8_t)(room_x - 1);
        entry_x = ROOM_MAX_X;
        transitioning = 1;
    } else if (px == ABS_MAX_X && has_east) {
        new_room_x = (uint8_t)(room_x + 1);
        entry_x = ROOM_MIN_X;
        transitioning = 1;
    } else if (py == ABS_MIN_Y && has_north) {
        new_room_y = (uint8_t)(room_y - 1);
        entry_y = ROOM_MAX_Y;
        transitioning = 1;
    } else if (py == ABS_MAX_Y && has_south) {
        new_room_y = (uint8_t)(room_y + 1);
        entry_y = ROOM_MIN_Y;
        transitioning = 1;
    }

    if (!transitioning) return;

    // A full room-tile-map rewrite mid-gameplay is the same "bulk VRAM
    // write while the LCD is live" risk category as any of this
    // project's other setup writes - guarded the same real-hardware-
    // safe way main.c's own boot sequence already is, rather than
    // accepting a rare tear.
    wait_vbl_done();
    DISPLAY_OFF;

    room_x = new_room_x;
    room_y = new_room_y;
    draw_current_room();
    player_set_position(entry_x, entry_y);

    SHOW_BKG;
    DISPLAY_ON;
}
