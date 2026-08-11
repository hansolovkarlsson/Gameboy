// See brute.h. A 16x16 circular blob - 4 8x8 quadrant tiles (TL/TR/BL/
// BR), the same per-pixel circular-distance-from-center technique
// enemy.c's own single 8x8 tile already uses, just at double the
// radius (center (7.5,7.5), radius 7.3 out of 16), generated and
// verified visually by a one-off local script before being
// transcribed here - real, computed shape, not hand-guessed bytes.
// One CGB OBJ palette, a violet/purple ramp distinct from the
// original enemy's red/orange "danger" coding, so the two read as
// different threats at a glance.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "brute.h"

#define BRUTE_TILE_TL 15 // player.c owns 0-11, sword.c owns 12-13, enemy.c owns 14
#define BRUTE_TILE_TR 16
#define BRUTE_TILE_BL 17
#define BRUTE_TILE_BR 18
#define BRUTE_TILE_COUNT 4

static const uint8_t brute_tiles[BRUTE_TILE_COUNT * 16] = {
    // TL
    0x00, 0x00, 0x07, 0x07, 0x1F, 0x1F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    // TR
    0x00, 0x00, 0xE0, 0xE0, 0xF8, 0xF8, 0xFC, 0xFC,
    0xFC, 0xFC, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    // BL
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x1F, 0x1F, 0x07, 0x07, 0x00, 0x00,
    // BR
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFC, 0xFC,
    0xFC, 0xFC, 0xF8, 0xF8, 0xE0, 0xE0, 0x00, 0x00,
};

#define BRUTE_PALETTE 6 // player.c owns 0, sword.c owns 1, enemy.c owns 2, heart_hud.c owns 3-4, key.c owns 5
static const palette_color_t brute_palette[4] = {
    RGB(0, 0, 0), RGB(10, 3, 14), RGB(16, 5, 20), RGB(22, 8, 28),
};

#define BRUTE_SPRITE_TL 11 // player.c owns 0-3, sword.c owns 4, enemy.c owns 5, heart_hud.c owns 6-8, pickup.c owns 9, key.c owns 10
#define BRUTE_SPRITE_TR 12
#define BRUTE_SPRITE_BL 13
#define BRUTE_SPRITE_BR 14

// Fixed vertical patrol range at a fixed x, comfortably inside room
// (2,1)'s open floor and away from any wall - vertical rather than
// enemy.c's horizontal, real visual variety between the two enemies
// from a one-axis swap. Bounds respect room.h's ROOM_MIN/MAX_X/Y (a
// 16x16 sprite's own safe range), same as the player.
#define BRUTE_X 64
#define BRUTE_MIN_Y 32
#define BRUTE_MAX_Y 88

// Two hits to die. The sword stays active for SWORD_FRAMES (12,
// sword.c) and world.c re-checks the hit-test every one of those
// frames - without a cooldown, one held swing would register up to 12
// hits, defeating "two hits" entirely. 20 frames is comfortably longer
// than 12 (so a single swing can never double-count) but short enough
// a real follow-up swing still lands at a natural pace.
#define BRUTE_MAX_HP 2
#define BRUTE_HIT_COOLDOWN_FRAMES 20

static uint8_t brute_y;
static int8_t brute_dir;
static uint8_t hp;
static uint8_t alive;
static uint8_t hit_cooldown;

static void position_brute(void) {
    move_sprite(BRUTE_SPRITE_TL, BRUTE_X + 8, brute_y + 16);
    move_sprite(BRUTE_SPRITE_TR, BRUTE_X + 8 + 8, brute_y + 16);
    move_sprite(BRUTE_SPRITE_BL, BRUTE_X + 8, brute_y + 16 + 8);
    move_sprite(BRUTE_SPRITE_BR, BRUTE_X + 8 + 8, brute_y + 16 + 8);
}

void brute_hide(void) {
    move_sprite(BRUTE_SPRITE_TL, 0, 0);
    move_sprite(BRUTE_SPRITE_TR, 0, 0);
    move_sprite(BRUTE_SPRITE_BL, 0, 0);
    move_sprite(BRUTE_SPRITE_BR, 0, 0);
}

void brute_init(void) {
    set_sprite_palette(BRUTE_PALETTE, 1, brute_palette);
    set_sprite_data(BRUTE_TILE_TL, BRUTE_TILE_COUNT, brute_tiles);
    set_sprite_prop(BRUTE_SPRITE_TL, S_PAL(BRUTE_PALETTE));
    set_sprite_prop(BRUTE_SPRITE_TR, S_PAL(BRUTE_PALETTE));
    set_sprite_prop(BRUTE_SPRITE_BL, S_PAL(BRUTE_PALETTE));
    set_sprite_prop(BRUTE_SPRITE_BR, S_PAL(BRUTE_PALETTE));
    set_sprite_tile(BRUTE_SPRITE_TL, BRUTE_TILE_TL);
    set_sprite_tile(BRUTE_SPRITE_TR, BRUTE_TILE_TR);
    set_sprite_tile(BRUTE_SPRITE_BL, BRUTE_TILE_BL);
    set_sprite_tile(BRUTE_SPRITE_BR, BRUTE_TILE_BR);

    brute_y = BRUTE_MIN_Y;
    brute_dir = 1;
    hp = BRUTE_MAX_HP;
    alive = 1;
    hit_cooldown = 0;

    // Unlike enemy_init() (room (0,0) is always also the boot room, so
    // its own first position_enemy() call already shows correctly),
    // room (2,1) is never the boot room - world.c's in_brute_room()
    // gate calling brute_update() only once the player actually enters
    // implicitly "shows" it, the same no-explicit-show mechanism
    // enemy.c already relies on, so a freshly-init'd brute must start
    // hidden here instead.
    brute_hide();
}

void brute_update(void) {
    if (hit_cooldown > 0) hit_cooldown--;
    if (!alive) return;

    uint8_t new_y = (uint8_t)(brute_y + brute_dir);
    if (new_y < BRUTE_MIN_Y || new_y > BRUTE_MAX_Y) {
        brute_dir = (int8_t)(-brute_dir);
        new_y = (uint8_t)(brute_y + brute_dir);
    }
    brute_y = new_y;

    position_brute();
}

uint8_t brute_try_hit(uint8_t hx, uint8_t hy, uint8_t hw, uint8_t hh) {
    if (!alive || hit_cooldown > 0) return 0;

    uint8_t overlap_x = hx < (uint8_t)(BRUTE_X + 16) && (uint8_t)(hx + hw) > BRUTE_X;
    uint8_t overlap_y = hy < (uint8_t)(brute_y + 16) && (uint8_t)(hy + hh) > brute_y;
    if (!overlap_x || !overlap_y) return 0;

    hp--;
    hit_cooldown = BRUTE_HIT_COOLDOWN_FRAMES;
    if (hp == 0) {
        alive = 0;
        brute_hide();
        return 2;
    }
    return 1;
}

uint8_t brute_get_x(void) { return BRUTE_X; }
uint8_t brute_get_y(void) { return brute_y; }
uint8_t brute_is_alive(void) { return alive; }

void brute_load_defeated(uint8_t defeated) {
    alive = !defeated;
    hp = defeated ? 0 : BRUTE_MAX_HP;
    if (defeated) brute_hide();
}
