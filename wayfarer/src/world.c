// See world.h. A 2x2 grid (GRID_W x GRID_H) of rooms, but *not* fully
// connected as a plain grid would be: (0,0)'s south side and (0,1)'s
// north side are permanently severed (never open, regardless of any
// key), turning what would otherwise be a 4-room cycle into a genuine
// dead-end chain - (0,0) -> (1,0) -> (1,1) -> (0,1) - so the one locked
// door (between (1,1) and (0,1), Milestone 5) actually gates progress
// instead of being a bypassable decoration. See room.c's own
// door_side/DOOR_* for the purely-cosmetic door-texture rendering;
// collision is still 100% driven by the has_* flags below, unchanged
// from Milestone 2's own already-correct per-side bound logic.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "enemy.h"
#include "heart_hud.h"
#include "key.h"
#include "pickup.h"
#include "player.h"
#include "room.h"
#include "sfx.h"
#include "sram.h"
#include "sword.h"
#include "win.h"
#include "world.h"

#define GRID_W 2
#define GRID_H 2

// The one enemy (enemy.c) belongs to this room only.
#define ENEMY_ROOM_X 0
#define ENEMY_ROOM_Y 0

// The one heart pickup (pickup.c) belongs to this room only.
#define PICKUP_ROOM_X 1
#define PICKUP_ROOM_Y 1

// The one key (key.c) belongs to this room only.
#define KEY_ROOM_X 1
#define KEY_ROOM_Y 0

// Winning requires both defeating the enemy and reaching this room -
// the same room Milestone 5's locked door gates, already informally
// named "the goal room" throughout that milestone's own comments.
#define WIN_ROOM_X 0
#define WIN_ROOM_Y 1

static uint8_t room_x;
static uint8_t room_y;
static uint8_t prev_joy;
static uint8_t won;

// Plain grid-topology defaults, then two overrides: a permanent sever
// (the (0,0)<->(0,1) edge, never open, not key-gated - see the file
// comment above) and a key-gated lock (the (1,1)<->(0,1) edge, open
// only once key_is_collected()). Used by both draw_current_room() and
// world_update()'s transition check - previously computed twice,
// independently, a real duplication now removed rather than repeated
// a third time for the new lock logic.
static void compute_sides(uint8_t rx, uint8_t ry, uint8_t *has_north, uint8_t *has_south, uint8_t *has_east, uint8_t *has_west) {
    *has_west = rx > 0;
    *has_east = rx < GRID_W - 1;
    *has_north = ry > 0;
    *has_south = ry < GRID_H - 1;

    if (rx == 0 && ry == 0) *has_south = 0;
    if (rx == 0 && ry == 1) *has_north = 0;

    if (rx == 1 && ry == 1) *has_west = key_is_collected();
    if (rx == 0 && ry == 1) *has_east = key_is_collected();
}

// Which side, if any, should render as a door texture when closed -
// purely cosmetic (room.c's own room_draw() doc comment).
static uint8_t door_side_for(uint8_t rx, uint8_t ry) {
    if (rx == 1 && ry == 1) return DOOR_WEST;
    if (rx == 0 && ry == 1) return DOOR_EAST;
    return DOOR_NONE;
}

static void draw_current_room(void) {
    uint8_t has_north, has_south, has_east, has_west;
    compute_sides(room_x, room_y, &has_north, &has_south, &has_east, &has_west);
    room_draw(has_north, has_south, has_east, has_west, door_side_for(room_x, room_y));
}

static uint8_t in_enemy_room(void) {
    return room_x == ENEMY_ROOM_X && room_y == ENEMY_ROOM_Y;
}

static uint8_t in_pickup_room(void) {
    return room_x == PICKUP_ROOM_X && room_y == PICKUP_ROOM_Y;
}

static uint8_t in_key_room(void) {
    return room_x == KEY_ROOM_X && room_y == KEY_ROOM_Y;
}

// The full room-switch sequence: a real-hardware-safe screen-off bulk
// redraw (same category of risk as any other bulk VRAM/tilemap write
// in this project - guarded the same way main.c's own boot sequence
// already is), moving the player to the new room's entry point, and
// showing/hiding the room-bound enemy/pickup/key sprites as
// appropriate. Used both by the normal edge-transition path and by
// the on-death respawn path below - a real, non-cosmetic factor-out:
// duplicating a DISPLAY_OFF/room-redraw block between the two would be
// exactly the kind of drift risk this project's own "guard
// real-hardware-safe timing carefully" discipline argues against.
static void go_to_room(uint8_t new_x, uint8_t new_y, uint8_t entry_x, uint8_t entry_y) {
    uint8_t leaving_enemy_room = in_enemy_room();
    uint8_t leaving_pickup_room = in_pickup_room();
    uint8_t leaving_key_room = in_key_room();

    wait_vbl_done();
    DISPLAY_OFF;

    room_x = new_x;
    room_y = new_y;
    draw_current_room();
    player_set_position(entry_x, entry_y);

    // Sprites persist across a BG-only room redraw unless explicitly
    // moved - a still-alive enemy/uncollected item must not linger on
    // screen after the player leaves its room, and the pickup/key (no
    // per-frame update of their own to fall back on, unlike the enemy)
    // need an explicit show on entry too.
    if (leaving_enemy_room && !in_enemy_room()) enemy_hide();
    if (leaving_pickup_room && !in_pickup_room()) pickup_hide();
    if (!leaving_pickup_room && in_pickup_room()) pickup_show();
    if (leaving_key_room && !in_key_room()) key_hide();
    if (!leaving_key_room && in_key_room()) key_show();

    SHOW_BKG;
    DISPLAY_ON;
}

void world_init(void) {
    room_x = 0;
    room_y = 0;
    prev_joy = 0;
    won = 0;

    sram_init();

    room_init(); // one-time tile/palette load
    draw_current_room();
    player_init();
    sword_init();
    enemy_init();
    enemy_load_defeated(sram_get_enemy_defeated());
    heart_hud_init();
    pickup_init();
    pickup_load_collected(sram_get_pickup_collected());
    key_init();
    key_load_collected(sram_get_key_collected());
    // Correct regardless of which room the game happens to start in,
    // not just assumed safe because today it's (0,0).
    if (in_pickup_room()) pickup_show(); else pickup_hide();
    if (in_key_room()) key_show(); else key_hide();

    // A loaded "already won" save shows the win screen immediately -
    // win_play()'s own DISPLAY_OFF/SHOW_BKG/DISPLAY_ON sequence simply
    // overdraws the room content prepared above before the display
    // ever turns on (still off from title_screen()'s own closing
    // DISPLAY_OFF), so there's no flicker of the fresh room first.
    if (sram_get_won()) {
        won = 1;
        win_play();
    }
}

void world_update(uint8_t joy) {
    // The win screen is a one-shot, terminal state - once shown,
    // nothing in this function runs again for the rest of the
    // session, matching this project's own "always fully resolve, no
    // half-finished states" style already used for the on-death
    // respawn path.
    if (won) return;

    player_update(joy);

    uint8_t has_north, has_south, has_east, has_west;
    compute_sides(room_x, room_y, &has_north, &has_south, &has_east, &has_west);

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
    uint8_t was_swinging = sword_is_active();
    sword_update(pressed & J_A);
    if ((pressed & J_A) && !was_swinging) sfx_play_swing();

    if (in_enemy_room()) {
        enemy_update();
        if (sword_is_active()) {
            if (enemy_try_hit(sword_get_x(), sword_get_y(), 8, 8)) {
                sfx_play_hit();
                sram_set_enemy_defeated();
            }
        }
        if (enemy_is_alive()) {
            uint8_t ex = enemy_get_x();
            uint8_t ey = enemy_get_y();
            uint8_t pcx = player_get_x();
            uint8_t pcy = player_get_y();
            uint8_t overlap_x = pcx < (uint8_t)(ex + 8) && (uint8_t)(pcx + 16) > ex;
            uint8_t overlap_y = pcy < (uint8_t)(ey + 8) && (uint8_t)(pcy + 16) > ey;
            if (overlap_x && overlap_y) {
                if (player_damage(1)) sfx_play_damage();
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
            sfx_play_pickup();
            sram_set_pickup_collected();
        }
    }

    if (in_key_room()) {
        if (key_try_collect(player_get_x(), player_get_y(), 16, 16)) {
            sfx_play_pickup();
            sram_set_key_collected();
        }
    }

    heart_hud_update();

    // Checked unconditionally every frame, after all the logic above,
    // rather than only at the specific moment either condition changes
    // - either the room-entry transition or the sword-kill earlier in
    // this same function could be the *second* of the two conditions
    // to become true.
    if (!won && !enemy_is_alive() && room_x == WIN_ROOM_X && room_y == WIN_ROOM_Y) {
        won = 1;
        sfx_play_win();
        win_play();
        sram_set_won();
    }
}
