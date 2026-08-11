// See world.h. A 3x2 grid (GRID_W x GRID_H) of rooms, but *not* fully
// connected as a plain grid would be: (0,0)'s south side and (0,1)'s
// north side are permanently severed (never open, regardless of any
// key) - so the one locked door (between (1,1) and (0,1), Milestone 5)
// actually gates progress to the win room instead of being a
// bypassable decoration. The other two columns ((1,0)/(1,1) and the
// Milestone 10 rooms (2,0)/(2,1)) stay plainly, fully connected -
// (1,0)'s and (1,1)'s own east sides open onto (2,0)/(2,1), forming a
// real explorable loop ((1,0)-(2,0)-(2,1)-(1,1)-(1,0)) alongside the
// linear critical path, rather than a dead-end appendage. (2,0) is
// still empty exploration space; (2,1) holds the brute (brute.c,
// Milestone 12) - a deliberately optional second enemy, not required
// to win. See room.c's own door_side/DOOR_* for the purely-cosmetic
// door-texture rendering; collision is still 100% driven by the has_*
// flags below, unchanged from Milestone 2's own already-correct
// per-side bound logic - confirmed general enough to grow the grid
// with zero other changes, not assumed.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "brute.h"
#include "enemy.h"
#include "heart_hud.h"
#include "key.h"
#include "music.h"
#include "pickup.h"
#include "player.h"
#include "room.h"
#include "sfx.h"
#include "shield.h"
#include "sram.h"
#include "sword.h"
#include "sword_pickup.h"
#include "win.h"
#include "world.h"

#define GRID_W 3
#define GRID_H 2

// The one enemy (enemy.c) belongs to this room only.
#define ENEMY_ROOM_X 0
#define ENEMY_ROOM_Y 0

// The sword pickup (sword_pickup.c) belongs to this same room -
// currently identical to ENEMY_ROOM_X/Y, but kept as its own named
// constant/helper rather than silently reusing in_enemy_room(): an
// explicit, separately-checked room membership can't silently break if
// either room is ever moved independently later.
#define SWORD_PICKUP_ROOM_X 0
#define SWORD_PICKUP_ROOM_Y 0

// The shield (shield.c) belongs to this room only - the last of the
// two rooms Milestone 10 left as empty exploration space.
#define SHIELD_ROOM_X 2
#define SHIELD_ROOM_Y 0

// The brute (brute.c) belongs to this room only - one of the two empty
// rooms Milestone 10 added. Deliberately optional: defeating it is not
// required to win (the win-condition check at the end of
// world_update() never consults it).
#define BRUTE_ROOM_X 2
#define BRUTE_ROOM_Y 1

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
// Edge-triggered block sfx - a block has no invincibility timer of its
// own to naturally gate repeat frames the way an unblocked hit already
// gets for free from player_damage()'s own invincible_timer, so these
// mirror the same "was_swinging" idiom this file already uses for the
// swing sfx. Kept independent, not shared, matching this project's own
// "don't factor two similar-but-independent blocks together" style
// already established for the enemy/brute contact blocks themselves.
static uint8_t was_blocking_enemy;
static uint8_t was_blocking_brute;

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

static uint8_t in_sword_pickup_room(void) {
    return room_x == SWORD_PICKUP_ROOM_X && room_y == SWORD_PICKUP_ROOM_Y;
}

static uint8_t in_shield_room(void) {
    return room_x == SHIELD_ROOM_X && room_y == SHIELD_ROOM_Y;
}

static uint8_t in_brute_room(void) {
    return room_x == BRUTE_ROOM_X && room_y == BRUTE_ROOM_Y;
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
    uint8_t leaving_brute_room = in_brute_room();
    uint8_t leaving_sword_pickup_room = in_sword_pickup_room();
    uint8_t leaving_shield_room = in_shield_room();
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
    if (leaving_brute_room && !in_brute_room()) brute_hide();
    if (leaving_sword_pickup_room && !in_sword_pickup_room()) sword_pickup_hide();
    if (!leaving_sword_pickup_room && in_sword_pickup_room()) sword_pickup_show();
    if (leaving_shield_room && !in_shield_room()) shield_hide();
    if (!leaving_shield_room && in_shield_room()) shield_show();
    if (leaving_pickup_room && !in_pickup_room()) pickup_hide();
    if (!leaving_pickup_room && in_pickup_room()) pickup_show();
    if (leaving_key_room && !in_key_room()) key_hide();
    if (!leaving_key_room && in_key_room()) key_show();

    SHOW_BKG;
    DISPLAY_ON;
}

// Everything needed to (re)start a session from room (0,0): resets
// every module's own state (via each one's normal _init()), loads
// whatever's currently in SRAM (a genuinely fresh cart's defaults on
// first boot, or real progress on a later one), and shows the win
// screen immediately if that state says the game's already been won.
// Shared by world_init() (the real first boot) and restart_game()
// (Milestone 11 - escaping a `won` save) rather than duplicated
// between them, since a second, drifting copy of this sequence would
// be exactly the kind of risk this project's own "guard real-
// hardware-safe timing carefully" discipline already argues against
// elsewhere.
static void reset_world(void) {
    room_x = 0;
    room_y = 0;
    won = 0;

    sram_init();
    music_init();

    room_init(); // one-time tile/palette load
    draw_current_room();
    player_init();
    sword_init();
    // Depends on sword_init() having already loaded the tile/palette
    // data this reuses - must run after it.
    sword_pickup_init();
    sword_pickup_load_collected(sram_get_sword_collected());
    shield_init();
    shield_load_collected(sram_get_shield_collected());
    enemy_init();
    enemy_load_defeated(sram_get_enemy_defeated());
    brute_init();
    brute_load_defeated(sram_get_brute_defeated());
    heart_hud_init();
    pickup_init();
    pickup_load_collected(sram_get_pickup_collected());
    key_init();
    key_load_collected(sram_get_key_collected());
    // Correct regardless of which room the game happens to start in,
    // not just assumed safe because today it's (0,0).
    if (in_sword_pickup_room()) sword_pickup_show(); else sword_pickup_hide();
    if (in_shield_room()) shield_show(); else shield_hide();
    if (in_pickup_room()) pickup_show(); else pickup_hide();
    if (in_key_room()) key_show(); else key_hide();

    // A loaded "already won" save shows the win screen immediately -
    // win_play()'s own DISPLAY_OFF/SHOW_BKG/DISPLAY_ON sequence simply
    // overdraws the room content prepared above before the display
    // ever turns on (still off from title_screen()'s own closing
    // DISPLAY_OFF, or restart_game()'s own just-issued DISPLAY_OFF),
    // so there's no flicker of the fresh room first.
    if (sram_get_won()) {
        won = 1;
        win_play();
    }
}

void world_init(void) {
    prev_joy = 0;
    reset_world();
}

// The only escape from a `won` state (Milestone 11) - wipes the
// persisted save back to fresh, then reruns the exact same setup a
// real first boot uses. Wrapped in the same real-hardware-safe
// screen-off pattern go_to_room()/win_play() already use, since this
// is a bulk visual reset happening mid-session (while the win screen
// is live), not at boot where the display is already off.
static void restart_game(void) {
    sram_reset();

    wait_vbl_done();
    DISPLAY_OFF;

    reset_world();

    SHOW_BKG;
    DISPLAY_ON;
}

void world_update(uint8_t joy) {
    // The win screen is otherwise a one-shot, terminal state - the
    // rest of this function never runs again once shown, matching
    // this project's own "always fully resolve, no half-finished
    // states" style already used for the on-death respawn path. The
    // one way out (Milestone 11): a Start press restarts a fresh
    // session. Scoped deliberately narrow to the actual reported
    // problem (being permanently stuck after winning and saving) -
    // not a general "Start restarts anytime" button during normal
    // play, unlike the sibling prism/ project's own always-available
    // restart.
    if (won) {
        uint8_t pressed = (uint8_t)(joy & (uint8_t)~prev_joy);
        prev_joy = joy;
        if (pressed & J_START) restart_game();
        return;
    }

    music_update();

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
    // Before the sword is collected, A does nothing at all - no blade
    // sprite, no sound - rather than swinging an empty hand. sword.c's
    // own timer simply never starts, so sword_is_active() already
    // reads false everywhere else below with zero changes needed there.
    if (sword_pickup_is_collected()) {
        uint8_t was_swinging = sword_is_active();
        sword_update(pressed & J_A);
        if ((pressed & J_A) && !was_swinging) sfx_play_swing();
    }

    if (in_sword_pickup_room() && !sword_pickup_is_collected()) {
        if (sword_pickup_try_collect(player_get_x(), player_get_y(), 16, 16)) {
            sfx_play_pickup();
            sram_set_sword_collected();
        }
    }

    if (in_shield_room() && !shield_is_collected()) {
        if (shield_try_collect(player_get_x(), player_get_y(), 16, 16)) {
            sfx_play_pickup();
            sram_set_shield_collected();
        }
    }

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
                uint8_t blocking = shield_blocks(pcx, pcy, 16, 16, ex, ey, 8, 8, player_get_facing());
                if (blocking) {
                    if (!was_blocking_enemy) sfx_play_block();
                } else {
                    if (player_damage(1)) sfx_play_damage();
                    if (player_get_hearts() == 0) {
                        go_to_room(ENEMY_ROOM_X, ENEMY_ROOM_Y, ROOM_CENTER_X, ROOM_CENTER_Y);
                        player_heal_full();
                    }
                }
                was_blocking_enemy = blocking;
            } else {
                was_blocking_enemy = 0;
            }
        }
    }

    if (in_brute_room()) {
        brute_update();
        if (sword_is_active()) {
            uint8_t hit = brute_try_hit(sword_get_x(), sword_get_y(), 8, 8);
            if (hit == 1) {
                sfx_play_brute_hit();
            } else if (hit == 2) {
                sfx_play_hit();
                sram_set_brute_defeated();
            }
        }
        if (brute_is_alive()) {
            uint8_t bx = brute_get_x();
            uint8_t by = brute_get_y();
            uint8_t pcx = player_get_x();
            uint8_t pcy = player_get_y();
            // 16x16 brute vs. 16x16 player - a plain symmetric AABB,
            // unlike the enemy block's own +8/+16 asymmetry (that
            // enemy is only 8x8).
            uint8_t overlap_x = pcx < (uint8_t)(bx + 16) && (uint8_t)(pcx + 16) > bx;
            uint8_t overlap_y = pcy < (uint8_t)(by + 16) && (uint8_t)(pcy + 16) > by;
            if (overlap_x && overlap_y) {
                uint8_t blocking = shield_blocks(pcx, pcy, 16, 16, bx, by, 16, 16, player_get_facing());
                if (blocking) {
                    if (!was_blocking_brute) sfx_play_block();
                } else {
                    if (player_damage(1)) sfx_play_damage();
                    if (player_get_hearts() == 0) {
                        go_to_room(BRUTE_ROOM_X, BRUTE_ROOM_Y, ROOM_CENTER_X, ROOM_CENTER_Y);
                        player_heal_full();
                    }
                }
                was_blocking_brute = blocking;
            } else {
                was_blocking_brute = 0;
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
        music_stop();
        sfx_play_win();
        win_play();
        sram_set_won();
    }
}
