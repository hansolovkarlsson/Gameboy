// See sram.h. Two bytes of real cartridge SRAM (gb/hardware.h's
// _SRAM[] array over the ENABLE_RAM-gated 0xA000-0xBFFF window,
// exactly the same mechanism the sibling prism/ project's own
// highscore.c already uses): a 1-byte magic value at _SRAM[0], then a
// single bitmask byte at _SRAM[1] - one bit per progress flag, the
// natural fit for 4 independent booleans (not a generalization for its
// own sake - still just 2 SRAM bytes total, smaller than prism/'s own
// 3). A magic-byte mismatch (including a genuinely fresh/uninitialized
// cart, which this emulator zero-fills) means "uninitialized" - never
// trust SRAM content blindly, the real-hardware-accurate practice, not
// an emulator-specific shortcut.

#include <gb/gb.h>
#include <stdint.h>

#include "sram.h"

#define SRAM_MAGIC 0x57
#define SRAM_MAGIC_ADDR 0
#define SRAM_STATE_ADDR 1

#define BIT_KEY 0x01
#define BIT_PICKUP 0x02
#define BIT_ENEMY 0x04
#define BIT_WON 0x08
#define BIT_BRUTE 0x10

static uint8_t state;

void sram_init(void) {
    ENABLE_RAM;
    if (_SRAM[SRAM_MAGIC_ADDR] != SRAM_MAGIC) {
        state = 0;
        _SRAM[SRAM_MAGIC_ADDR] = SRAM_MAGIC;
        _SRAM[SRAM_STATE_ADDR] = state;
    } else {
        state = _SRAM[SRAM_STATE_ADDR];
    }
    DISABLE_RAM;
}

uint8_t sram_get_key_collected(void) { return (state & BIT_KEY) != 0; }
uint8_t sram_get_pickup_collected(void) { return (state & BIT_PICKUP) != 0; }
uint8_t sram_get_enemy_defeated(void) { return (state & BIT_ENEMY) != 0; }
uint8_t sram_get_won(void) { return (state & BIT_WON) != 0; }
uint8_t sram_get_brute_defeated(void) { return (state & BIT_BRUTE) != 0; }

static void save(void) {
    ENABLE_RAM;
    _SRAM[SRAM_MAGIC_ADDR] = SRAM_MAGIC;
    _SRAM[SRAM_STATE_ADDR] = state;
    DISABLE_RAM;
}

void sram_set_key_collected(void) { state |= BIT_KEY; save(); }
void sram_set_pickup_collected(void) { state |= BIT_PICKUP; save(); }
void sram_set_enemy_defeated(void) { state |= BIT_ENEMY; save(); }
void sram_set_won(void) { state |= BIT_WON; save(); }
void sram_set_brute_defeated(void) { state |= BIT_BRUTE; save(); }

void sram_reset(void) {
    state = 0;
    save();
}
