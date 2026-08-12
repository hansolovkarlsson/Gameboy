// See sram.h. Three bytes of real cartridge SRAM (gb/hardware.h's
// _SRAM[] array over the ENABLE_RAM-gated 0xA000-0xBFFF window,
// exactly the same mechanism the sibling prism/ project's own
// highscore.c already uses): a 1-byte magic value at _SRAM[0], then two
// bitmask bytes - _SRAM[1] (the original 8 progress flags, now
// completely full) and _SRAM[2] (Milestone 17's chest, the first flag
// that needed a second byte, one bit used of 8 available). A magic-byte
// mismatch (including a genuinely fresh/uninitialized cart, which this
// emulator zero-fills) means "uninitialized" - never trust SRAM content
// blindly, the real-hardware-accurate practice, not an
// emulator-specific shortcut.

#include <gb/gb.h>
#include <stdint.h>

#include "sram.h"

#define SRAM_MAGIC 0x57
#define SRAM_MAGIC_ADDR 0
#define SRAM_STATE_ADDR 1
#define SRAM_STATE2_ADDR 2

#define BIT_KEY 0x01
#define BIT_PICKUP 0x02
#define BIT_ENEMY 0x04
#define BIT_WON 0x08
#define BIT_BRUTE 0x10
#define BIT_SWORD 0x20
#define BIT_SHIELD 0x40
#define BIT_BOSS 0x80 // the 8th and last bit available in _SRAM[1] - any further flag goes in _SRAM[2]'s own state2 instead, not silently assumed to still fit here

#define BIT_CHEST 0x01 // the first bit of the second state byte, _SRAM[2]

static uint8_t state;
static uint8_t state2;

void sram_init(void) {
    ENABLE_RAM;
    if (_SRAM[SRAM_MAGIC_ADDR] != SRAM_MAGIC) {
        state = 0;
        state2 = 0;
        _SRAM[SRAM_MAGIC_ADDR] = SRAM_MAGIC;
        _SRAM[SRAM_STATE_ADDR] = state;
        _SRAM[SRAM_STATE2_ADDR] = state2;
    } else {
        state = _SRAM[SRAM_STATE_ADDR];
        state2 = _SRAM[SRAM_STATE2_ADDR];
    }
    DISABLE_RAM;
}

uint8_t sram_get_key_collected(void) { return (state & BIT_KEY) != 0; }
uint8_t sram_get_pickup_collected(void) { return (state & BIT_PICKUP) != 0; }
uint8_t sram_get_enemy_defeated(void) { return (state & BIT_ENEMY) != 0; }
uint8_t sram_get_won(void) { return (state & BIT_WON) != 0; }
uint8_t sram_get_brute_defeated(void) { return (state & BIT_BRUTE) != 0; }
uint8_t sram_get_sword_collected(void) { return (state & BIT_SWORD) != 0; }
uint8_t sram_get_shield_collected(void) { return (state & BIT_SHIELD) != 0; }
uint8_t sram_get_boss_defeated(void) { return (state & BIT_BOSS) != 0; }
uint8_t sram_get_chest_collected(void) { return (state2 & BIT_CHEST) != 0; }

static void save(void) {
    ENABLE_RAM;
    _SRAM[SRAM_MAGIC_ADDR] = SRAM_MAGIC;
    _SRAM[SRAM_STATE_ADDR] = state;
    _SRAM[SRAM_STATE2_ADDR] = state2;
    DISABLE_RAM;
}

void sram_set_key_collected(void) { state |= BIT_KEY; save(); }
void sram_set_pickup_collected(void) { state |= BIT_PICKUP; save(); }
void sram_set_enemy_defeated(void) { state |= BIT_ENEMY; save(); }
void sram_set_won(void) { state |= BIT_WON; save(); }
void sram_set_brute_defeated(void) { state |= BIT_BRUTE; save(); }
void sram_set_sword_collected(void) { state |= BIT_SWORD; save(); }
void sram_set_shield_collected(void) { state |= BIT_SHIELD; save(); }
void sram_set_boss_defeated(void) { state |= BIT_BOSS; save(); }
void sram_set_chest_collected(void) { state2 |= BIT_CHEST; save(); }

void sram_reset(void) {
    state = 0;
    state2 = 0;
    save();
}
