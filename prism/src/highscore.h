#ifndef PRISM_HIGHSCORE_H
#define PRISM_HIGHSCORE_H
#include <stdint.h>

// Persists the best score seen so far to real cartridge SRAM (see
// prism/Makefile's -Wl-yt0x03/-Wm-ya1 flags and prism/src/main.c's own
// initialization order comment). Must be called after
// gb_cart_load_ram_file() has had a chance to run - i.e., only in a
// real emulator/hardware boot, never assumed valid without the magic
// byte check below.
void highscore_init(void);
uint16_t highscore_get(void);

// Updates the cached value and writes it to SRAM only if `score` beats
// the current high score - a no-op otherwise, so a losing/tying game
// never touches SRAM at all.
void highscore_maybe_update(uint16_t score);

#endif
