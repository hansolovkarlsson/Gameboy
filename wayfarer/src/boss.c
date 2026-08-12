// See boss.h. A 24x24 circular blob - 9 8x8 quadrant tiles (TL/TM/TR/
// ML/MM/MR/BL/BM/BR), the same per-pixel circular-distance-from-center
// technique enemy.c's and brute.c's own blobs already use, a third
// application at a bigger radius (center (11.5,11.5), radius 11 out of
// 24), generated and verified visually by a one-off local script
// before being transcribed here - real, computed shape, not hand-
// guessed bytes.
//
// Deliberately reuses brute.c's own OBJ palette (BRUTE_OBJ_PALETTE,
// exported via brute.h) rather than a new one of its own - a real
// hardware constraint found the hard way, not a stylistic choice:
// CGB has only 8 OBJ palette slots total (confirmed against
// docs/HARDWARE_REFERENCE.md), and every existing module here already
// claims one (player 0, sword 1, enemy 2, heart_hud 3-4, key 5,
// brute 6, shield 7) - all 8 were already spoken for before this
// milestone even started. An earlier draft tried claiming a 9th
// (index 8) anyway; CGB's OCPS palette-RAM index is only 6 bits wide,
// so that write silently wrapped around and overwrote *palette 0* -
// the player's own colors - visible immediately as the player sprite
// rendering in the boss's own crimson the moment it was actually
// tested, not caught by reasoning about the code alone.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "boss.h"
#include "brute.h"

#define BOSS_TILE_TL 22 // player.c owns 0-11, sword.c owns 12-13, enemy.c owns 14, heart_hud.c owns 15, key.c owns 16, brute.c owns 17-20, shield.c owns 21
#define BOSS_TILE_TM 23
#define BOSS_TILE_TR 24
#define BOSS_TILE_ML 25
#define BOSS_TILE_MM 26
#define BOSS_TILE_MR 27
#define BOSS_TILE_BL 28
#define BOSS_TILE_BM 29
#define BOSS_TILE_BR 30
#define BOSS_TILE_COUNT 9

static const uint8_t boss_tiles[BOSS_TILE_COUNT * 16] = {
    // TL
    0x00, 0x00, 0x03, 0x03, 0x07, 0x07, 0x0F, 0x0F,
    0x1F, 0x1F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    // TM
    0x00, 0x00, 0x7E, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // TR
    0x00, 0x00, 0xC0, 0xC0, 0xE0, 0xE0, 0xF0, 0xF0,
    0xF8, 0xF8, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC,
    // ML
    0x3F, 0x3F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3F, 0x3F,
    // MM
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // MR
    0xFC, 0xFC, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFC, 0xFC,
    // BL
    0x3F, 0x3F, 0x3F, 0x3F, 0x1F, 0x1F, 0x0F, 0x0F,
    0x07, 0x07, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
    // BM
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x7E, 0x00, 0x00,
    // BR
    0xFC, 0xFC, 0xFC, 0xFC, 0xF8, 0xF8, 0xF0, 0xF0,
    0xE0, 0xE0, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00,
};

#define BOSS_SPRITE_TL 17 // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, heart_hud.c owns 6-8, pickup.c owns 9, key.c owns 10, brute.c owns 11-14, sword_pickup.c owns 15, shield.c owns 16
#define BOSS_SPRITE_TM 18
#define BOSS_SPRITE_TR 19
#define BOSS_SPRITE_ML 20
#define BOSS_SPRITE_MM 21
#define BOSS_SPRITE_MR 22
#define BOSS_SPRITE_BL 23
#define BOSS_SPRITE_BM 24
#define BOSS_SPRITE_BR 25

// Fixed patrol box in room (2,2), comfortably inside room.h's own
// ROOM_MIN/MAX_X/Y - those constants are calibrated for a 16x16
// sprite, so a 24px-wide/tall body needs its own tighter margin, not
// a direct reuse (152-24=128 right edge margin, 136-24=112 bottom
// edge margin, vs. the 16px sprite's own 136/120).
#define BOSS_MIN_X 24
#define BOSS_MAX_X 96
#define BOSS_MIN_Y 16
#define BOSS_MAX_Y 80

// Three hits to die, same post-hit-cooldown reasoning brute.c's own
// BRUTE_HIT_COOLDOWN_FRAMES already established: comfortably longer
// than sword.c's own SWORD_FRAMES (12) so one held swing can never
// register more than once.
#define BOSS_MAX_HP 3
#define BOSS_HIT_COOLDOWN_FRAMES 20

static uint8_t boss_x;
static uint8_t boss_y;
static int8_t boss_dir_x;
static int8_t boss_dir_y;
static uint8_t hp;
static uint8_t alive;
static uint8_t hit_cooldown;

static void position_boss(void) {
    move_sprite(BOSS_SPRITE_TL, boss_x + 8, boss_y + 16);
    move_sprite(BOSS_SPRITE_TM, boss_x + 8 + 8, boss_y + 16);
    move_sprite(BOSS_SPRITE_TR, boss_x + 8 + 16, boss_y + 16);
    move_sprite(BOSS_SPRITE_ML, boss_x + 8, boss_y + 16 + 8);
    move_sprite(BOSS_SPRITE_MM, boss_x + 8 + 8, boss_y + 16 + 8);
    move_sprite(BOSS_SPRITE_MR, boss_x + 8 + 16, boss_y + 16 + 8);
    move_sprite(BOSS_SPRITE_BL, boss_x + 8, boss_y + 16 + 16);
    move_sprite(BOSS_SPRITE_BM, boss_x + 8 + 8, boss_y + 16 + 16);
    move_sprite(BOSS_SPRITE_BR, boss_x + 8 + 16, boss_y + 16 + 16);
}

void boss_hide(void) {
    move_sprite(BOSS_SPRITE_TL, 0, 0);
    move_sprite(BOSS_SPRITE_TM, 0, 0);
    move_sprite(BOSS_SPRITE_TR, 0, 0);
    move_sprite(BOSS_SPRITE_ML, 0, 0);
    move_sprite(BOSS_SPRITE_MM, 0, 0);
    move_sprite(BOSS_SPRITE_MR, 0, 0);
    move_sprite(BOSS_SPRITE_BL, 0, 0);
    move_sprite(BOSS_SPRITE_BM, 0, 0);
    move_sprite(BOSS_SPRITE_BR, 0, 0);
}

void boss_init(void) {
    // No set_sprite_palette() call here - BRUTE_OBJ_PALETTE's own
    // color data is already loaded by brute_init(), which reset_world()
    // always calls before this (world.c's own call order); reusing the
    // same palette index needs it set on this module's own sprites, not
    // reloaded a second time.
    set_sprite_data(BOSS_TILE_TL, BOSS_TILE_COUNT, boss_tiles);
    set_sprite_prop(BOSS_SPRITE_TL, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_TM, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_TR, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_ML, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_MM, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_MR, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_BL, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_BM, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_prop(BOSS_SPRITE_BR, S_PAL(BRUTE_OBJ_PALETTE));
    set_sprite_tile(BOSS_SPRITE_TL, BOSS_TILE_TL);
    set_sprite_tile(BOSS_SPRITE_TM, BOSS_TILE_TM);
    set_sprite_tile(BOSS_SPRITE_TR, BOSS_TILE_TR);
    set_sprite_tile(BOSS_SPRITE_ML, BOSS_TILE_ML);
    set_sprite_tile(BOSS_SPRITE_MM, BOSS_TILE_MM);
    set_sprite_tile(BOSS_SPRITE_MR, BOSS_TILE_MR);
    set_sprite_tile(BOSS_SPRITE_BL, BOSS_TILE_BL);
    set_sprite_tile(BOSS_SPRITE_BM, BOSS_TILE_BM);
    set_sprite_tile(BOSS_SPRITE_BR, BOSS_TILE_BR);

    boss_x = BOSS_MIN_X;
    boss_y = BOSS_MIN_Y;
    boss_dir_x = 1;
    boss_dir_y = 1;
    hp = BOSS_MAX_HP;
    alive = 1;
    hit_cooldown = 0;

    boss_hide();
}

void boss_update(void) {
    if (hit_cooldown > 0) hit_cooldown--;
    if (!alive) return;

    uint8_t new_x = (uint8_t)(boss_x + boss_dir_x);
    if (new_x < BOSS_MIN_X || new_x > BOSS_MAX_X) {
        boss_dir_x = (int8_t)(-boss_dir_x);
        new_x = (uint8_t)(boss_x + boss_dir_x);
    }
    boss_x = new_x;

    uint8_t new_y = (uint8_t)(boss_y + boss_dir_y);
    if (new_y < BOSS_MIN_Y || new_y > BOSS_MAX_Y) {
        boss_dir_y = (int8_t)(-boss_dir_y);
        new_y = (uint8_t)(boss_y + boss_dir_y);
    }
    boss_y = new_y;

    position_boss();
}

uint8_t boss_try_hit(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (!alive || hit_cooldown > 0) return 0;

    uint8_t overlap_x = hx < (uint8_t)(boss_x + 24) && (uint8_t)(hx + hw) > boss_x;
    uint8_t overlap_y = hy < (uint8_t)(boss_y + 24) && (uint8_t)(hy + hh) > boss_y;
    if (!overlap_x || !overlap_y) return 0;

    hp--;
    hit_cooldown = BOSS_HIT_COOLDOWN_FRAMES;
    if (hp == 0) {
        alive = 0;
        boss_hide();
        return 2;
    }
    return 1;
}

uint8_t boss_get_x(void) { return boss_x; }
uint8_t boss_get_y(void) { return boss_y; }
uint8_t boss_is_alive(void) { return alive; }

void boss_load_defeated(uint8_t defeated) {
    alive = !defeated;
    hp = defeated ? 0 : BOSS_MAX_HP;
    if (defeated) boss_hide();
}
