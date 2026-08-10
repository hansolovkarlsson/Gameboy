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

#include "enemy.h"
#include "heart_hud.h"
#include "pickup.h"
#include "player.h"
#include "room.h"
#include "sword.h"
#include "world.h"

#define GRID_W 2
#define GRID_H 2

// The one enemy (enemy.c) belongs to this room only.
#define ENEMY_ROOM_X 0
#define ENEMY_ROOM_Y 0

// The one heart pickup (pickup.c) belongs to this room only.
#define PICKUP_ROOM_X 1
#define PICKUP_ROOM_Y 1

static uint8_t room_x;
static uint8_t room_y;
static uint8_t prev_joy;

static void draw_current_room(void) {
    uint8_t has_west = room_x > 0;
    uint8_t has_east = room_x < GRID_W - 1;
    uint8_t has_north = room_y > 0;
    uint8_t has_south = room_y < GRID_H - 1;
    room_draw(has_north, has_south, has_east, has_west);
}

static uint8_t in_enemy_room(void) {
    return room_x == ENEMY_ROOM_X && room_y == ENEMY_ROOM_Y;
}

static uint8_t in_pickup_room(void) {
    return room_x == PICKUP_ROOM_X && room_y == PICKUP_ROOM_Y;
}

// The full room-switch sequence: a real-hardware-safe screen-off bulk
// redraw (same category of risk as any other bulk VRAM/tilemap write
// in this project - guarded the same way main.c's own boot sequence
// already is), moving the player to the new room's entry point, and
// showing/hiding the room-bound enemy/pickup sprites as appropriate.
// Used both by the normal edge-transition path and by the on-death
// respawn path below - a real, non-cosmetic factor-out: duplicating a
// DISPLAY_OFF/room-redraw block between the two would be exactly the
// kind of drift risk this project's own "guard real-hardware-safe
// timing carefully" discipline argues against.
static void go_to_room(uint8_t new_x, uint8_t new_y, uint8_t entry_x, uint8_t entry_y) {
    uint8_t leaving_enemy_room = in_enemy_room();
    uint8_t leaving_pickup_room = in_pickup_room();

    wait_vbl_done();
    DISPLAY_OFF;

    room_x = new_x;
    room_y = new_y;
    draw_current_room();
    player_set_position(entry_x, entry_y);

    // Sprites persist across a BG-only room redraw unless explicitly
    // moved - a still-alive enemy/uncollected pickup must not linger
    // on screen after the player leaves its room, and the pickup (no
    // per-frame update of its own to fall back on, unlike the enemy)
    // needs an explicit show on entry too.
    if (leaving_enemy_room && !in_enemy_room()) enemy_hide();
    if (leaving_pickup_room && !in_pickup_room()) pickup_hide();
    if (!leaving_pickup_room && in_pickup_room()) pickup_show();

    SHOW_BKG;
    DISPLAY_ON;
}

void world_init(void) {
    room_x = 0;
    room_y = 0;
    prev_joy = 0;
    room_init(); // one-time tile/palette load
    draw_current_room();
    player_init();
    sword_init();
    enemy_init();
    heart_hud_init();
    pickup_init();
    // Correct regardless of which room the game happens to start in,
    // not just assumed safe because today it's (0,0).
    if (in_pickup_room()) pickup_show(); else pickup_hide();
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

    if (transitioning) {
        go_to_room(new_room_x, new_room_y, entry_x, entry_y);
    }

    uint8_t pressed = (uint8_t)(joy & (uint8_t)~prev_joy);
    prev_joy = joy;
    sword_update(pressed & J_A);

    if (in_enemy_room()) {
        enemy_update();
        if (sword_is_active()) {
            enemy_try_hit(sword_get_x(), sword_get_y(), 8, 8);
        }
        if (enemy_is_alive()) {
            uint8_t ex = enemy_get_x();
            uint8_t ey = enemy_get_y();
            uint8_t pcx = player_get_x();
            uint8_t pcy = player_get_y();
            uint8_t overlap_x = pcx < (uint8_t)(ex + 8) && (uint8_t)(pcx + 16) > ex;
            uint8_t overlap_y = pcy < (uint8_t)(ey + 8) && (uint8_t)(pcy + 16) > ey;
            if (overlap_x && overlap_y) {
                player_damage(1);
                if (player_get_hearts() == 0) {
                    go_to_room(ENEMY_ROOM_X, ENEMY_ROOM_Y, ROOM_CENTER_X, ROOM_CENTER_Y);
                    player_heal_full();
                }
            }
        }
    }

    if (in_pickup_room()) {
        if (pickup_try_collect(player_get_x(), player_get_y(), 16, 16)) {
            player_heal_full();
        }
    }

    heart_hud_update();
}
