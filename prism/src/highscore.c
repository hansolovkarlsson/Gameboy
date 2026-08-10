// See highscore.h. Three bytes of real cartridge SRAM
// (gb/hardware.h's _SRAM[] array over the ENABLE_RAM-gated
// 0xA000-0xBFFF window): a 1-byte magic value at _SRAM[0], then the
// uint16_t high score as two explicit little-endian bytes at
// _SRAM[1]/_SRAM[2] - explicit byte packing rather than a cast through
// a uint16_t*, matching this codebase's general preference for simple,
// explicit state manipulation over relying on implicit compiler
// behavior. A magic-byte mismatch (including a genuinely fresh/
// uninitialized cart, which this emulator zero-fills) means
// "uninitialized" - never trust SRAM content blindly, the real-
// hardware-accurate practice, not an emulator-specific shortcut.

#include <gb/gb.h>
#include <stdint.h>

#include "highscore.h"

#define SRAM_MAGIC 0x48
#define SRAM_MAGIC_ADDR 0
#define SRAM_SCORE_LOW_ADDR 1
#define SRAM_SCORE_HIGH_ADDR 2

static uint16_t cached_high_score = 0;

void highscore_init(void) {
    ENABLE_RAM;
    if (_SRAM[SRAM_MAGIC_ADDR] != SRAM_MAGIC) {
        cached_high_score = 0;
        _SRAM[SRAM_MAGIC_ADDR] = SRAM_MAGIC;
        _SRAM[SRAM_SCORE_LOW_ADDR] = 0;
        _SRAM[SRAM_SCORE_HIGH_ADDR] = 0;
    } else {
        cached_high_score = (uint16_t)(_SRAM[SRAM_SCORE_LOW_ADDR] |
                                        ((uint16_t)_SRAM[SRAM_SCORE_HIGH_ADDR] << 8));
    }
    DISABLE_RAM;
}

uint16_t highscore_get(void) {
    return cached_high_score;
}

void highscore_maybe_update(uint16_t score) {
    if (score <= cached_high_score) return;
    cached_high_score = score;

    ENABLE_RAM;
    _SRAM[SRAM_MAGIC_ADDR] = SRAM_MAGIC;
    _SRAM[SRAM_SCORE_LOW_ADDR] = (uint8_t)(score & 0xFF);
    _SRAM[SRAM_SCORE_HIGH_ADDR] = (uint8_t)(score >> 8);
    DISABLE_RAM;
}
