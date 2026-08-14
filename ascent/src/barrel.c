// See barrel.h. Up to MAX_BARRELS 8x8 barrels. Each is either rolling
// (moving horizontally 1px/frame, the same speed as the player's own
// walk) or descending (falling 1px/frame straight down through a
// ladder column, the same speed as the player's own gravity).
//
// A rolling barrel checks stage_is_ladder() at the tile it's resting
// on (its own feet row, one row below its box - the same "just below
// the box" offset player.c's own gravity check uses, not the "center"
// offset the player's *climb* check uses, since that offset is tuned
// for a 16px-tall player and doesn't land on anything meaningful for
// an 8px barrel). Reaching a ladder-top tile switches it to
// descending; landing - stage_is_solid() at its feet, but only once
// it has actually moved into a *different* tile row than the one it
// started descending from (the starting row is itself solid, being a
// ladder-top junction, so checking landing there immediately would
// never let it actually fall) - switches it back to rolling and
// *reverses its direction*. That one reversal rule is enough to
// retrace the player's own stage.c climb route in reverse with no
// per-tier special-casing - see docs/GAMEBOY_ROADMAP.md's own
// Milestone 2 entry for the full column-by-column trace confirming
// this against the real tile map.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "barrel.h"
#include "stage.h"

#define MAX_BARRELS 2
// ~7.5s at ~60fps. Empirically, the player's own full ground-to-top
// climb (stage.c's zigzag route, three ladder legs) takes ~374 frames
// of real, unhurried play - the first barrel needs to spawn with
// enough grace period after that for a fresh climb to reasonably
// reach the top before any barrel is even in play, matching real
// Donkey Kong's own brief delay before the first barrel appears
// rather than dropping one before the player can realistically climb
// anywhere. Confirmed against the real build (see
// docs/GAMEBOY_ROADMAP.md's own Milestone 2 entry) - a shorter
// interval was tried first and found to let a barrel's own transit
// down column 4 or column 14 collide with the player's *own* first
// climb up those same columns, before the player has any barrel to
// react to yet.
#define SPAWN_INTERVAL 450

#define BARREL_TILE_ID 4

// Barrel silhouette: two dark "hoop" bands (rows 3 and 6) around a tan
// fill, a narrower rounded rim top/bottom (rows 0/7) - the same
// row-bitmask 2-color-plus-transparent convention (color 0 hardware-
// transparent, same as player.c's own unused OBJ color 0) every
// hand-drawn tile in this project already relies on.
static const uint8_t barrel_tile[16] = {
    0x7E, 0x00, // rounded rim
    0x81, 0x7E, // side border, tan fill
    0x81, 0x7E,
    0xFF, 0x00, // hoop band
    0x81, 0x7E,
    0x81, 0x7E,
    0xFF, 0x00, // hoop band
    0x7E, 0x00, // rounded rim
};

// One CGB OBJ palette: color 0 unused (hardware-transparent), color 1
// a dark hoop/border brown, color 2 a warm wood-tan fill, color 3
// unused. Palette 0 belongs to the player (player.c) - CGB has 8 OBJ
// palette slots, plenty of room for a second.
#define BARREL_PALETTE 1
static const palette_color_t barrel_palette[4] = {
    RGB(0, 0, 0), RGB(10, 6, 2), RGB(22, 16, 6), RGB(0, 0, 0),
};

// Tier 3 (the top platform) is stage.c's own row 5 - see stage.c's own
// tile map. Barrels spawn resting on it, an 8px-tall box flush with
// the girder's surface.
#define TIER3_TILE_ROW 5
#define BARREL_SPAWN_X 144
#define BARREL_SPAWN_Y (TIER3_TILE_ROW * 8 - 8)

typedef struct {
    uint8_t active;
    uint8_t x;
    uint8_t y;
    int8_t dir;
    uint8_t descending;
    // The tile row the barrel was resting on when it started
    // descending - landing is only checked once the barrel has moved
    // into a *different* row, since the row it started from is itself
    // solid (that's what a ladder-top junction tile means: solid to
    // stand on, but a barrel arriving there while rolling should pass
    // through it going down, not land on it immediately).
    uint8_t descend_start_row;
    // Set once barrel_check_jump_score() has paid out for this barrel
    // (see barrel.h) - a fresh barrel spawn (spawn_barrel()) always
    // clears it, so a reused slot's old barrel can never block a new
    // one's own bonus.
    uint8_t jumped;
} barrel_t;

static barrel_t barrels[MAX_BARRELS];
static uint16_t spawn_timer;

static const uint8_t barrel_sprite_slot[MAX_BARRELS] = {4, 5};

static void position_barrel(uint8_t i) {
    uint8_t slot = barrel_sprite_slot[i];
    if (barrels[i].active) {
        set_sprite_tile(slot, BARREL_TILE_ID);
        move_sprite(slot, barrels[i].x + 8, barrels[i].y + 16);
    } else {
        move_sprite(slot, 0, 0);
    }
}

void barrel_init(void) {
    set_sprite_palette(BARREL_PALETTE, 1, barrel_palette);
    set_sprite_data(BARREL_TILE_ID, 1, barrel_tile);

    spawn_timer = 0;
    for (uint8_t i = 0; i < MAX_BARRELS; i++) {
        barrels[i].active = 0;
        set_sprite_prop(barrel_sprite_slot[i], BARREL_PALETTE);
        position_barrel(i);
    }
}

static void spawn_barrel(void) {
    for (uint8_t i = 0; i < MAX_BARRELS; i++) {
        if (!barrels[i].active) {
            barrels[i].active = 1;
            barrels[i].x = BARREL_SPAWN_X;
            barrels[i].y = BARREL_SPAWN_Y;
            barrels[i].dir = -1;
            barrels[i].descending = 0;
            barrels[i].jumped = 0;
            return;
        }
    }
}

void barrel_update(void) {
    spawn_timer++;
    if (spawn_timer >= SPAWN_INTERVAL) {
        spawn_timer = 0;
        spawn_barrel();
    }

    for (uint8_t i = 0; i < MAX_BARRELS; i++) {
        if (!barrels[i].active) continue;

        if (barrels[i].descending) {
            barrels[i].y++;
            uint8_t tile_row = (uint8_t)((barrels[i].y + 8) / 8);
            if (tile_row != barrels[i].descend_start_row &&
                stage_is_solid(barrels[i].x + 4, barrels[i].y + 8)) {
                barrels[i].y = (uint8_t)(tile_row * 8 - 8);
                barrels[i].dir = (int8_t)(-barrels[i].dir);
                barrels[i].descending = 0;
            }
        } else {
            barrels[i].x = (uint8_t)(barrels[i].x + barrels[i].dir);
            if (barrels[i].x == 0 || barrels[i].x >= 152) {
                barrels[i].active = 0;
            } else if (stage_is_ladder(barrels[i].x + 4, barrels[i].y + 8)) {
                barrels[i].descending = 1;
                barrels[i].descend_start_row = (uint8_t)((barrels[i].y + 8) / 8);
            }
        }

        position_barrel(i);
    }
}

uint8_t barrel_check_hit(uint8_t player_x, uint8_t player_y) {
    for (uint8_t i = 0; i < MAX_BARRELS; i++) {
        if (!barrels[i].active) continue;
        if (player_x < barrels[i].x + 8 && player_x + 16 > barrels[i].x &&
            player_y < barrels[i].y + 8 && player_y + 16 > barrels[i].y) {
            return 1;
        }
    }
    return 0;
}

// 100 points/barrel - the same value real Donkey Kong (1981) awards
// for a jumped barrel, not picked arbitrarily.
#define JUMP_SCORE_POINTS 100

// A barrel more than this many pixels away (feet-to-feet) from the
// player's own vertical position is on a different tier entirely, not
// one the player could plausibly be jumping over right now - tiers are
// 32px apart (stage.c's own rows 5/9/13/17), a 20px jump arc
// (player.c's own JUMP_RISE_FRAMES) never reaches a neighboring one, so
// this margin only ever matches a barrel genuinely on the player's own
// tier.
#define JUMP_SCORE_MAX_DY 24

uint16_t barrel_check_jump_score(uint8_t player_x, uint8_t player_y) {
    uint16_t points = 0;
    uint8_t player_feet_y = player_y + 16;
    for (uint8_t i = 0; i < MAX_BARRELS; i++) {
        if (!barrels[i].active || barrels[i].jumped) continue;
        uint8_t barrel_feet_y = barrels[i].y + 8;
        uint8_t dy = player_feet_y > barrel_feet_y
                         ? player_feet_y - barrel_feet_y
                         : barrel_feet_y - player_feet_y;
        if (dy > JUMP_SCORE_MAX_DY) continue;
        if (player_x < barrels[i].x + 8 && player_x + 16 > barrels[i].x) {
            barrels[i].jumped = 1;
            points = (uint16_t)(points + JUMP_SCORE_POINTS);
        }
    }
    return points;
}
