// See sfx.h. Direct DMG/CGB sound-register pokes, same approach and
// register-level grounding as the sibling prism/ and wayfarer/
// projects' own sfx.c (this emulator's own APU - src/apu.c - already
// fully implemented and verified, tests/test_apu.c and
// test_roms/droneboy). Three of the four tones below reuse those
// projects' own exact register values verbatim rather than re-deriving
// them - the same physical channel-1/2/4 hardware, the same
// frequency-to-period formula, so there's nothing to recompute, only
// to reuse (same "already-proven, don't redo it" reasoning player.c/
// win.c/score.c already give for reusing wayfarer's/prism's own art).

#include <gb/gb.h>
#include <stdint.h>

// Channels 1/2's 11-bit "period" register (NRx3 low 8 bits + NRx4 low
// 3 bits) relates to real frequency via pandocs' own conversion:
// frequency = 131072 / (2048 - period). Values below are the exact
// periods wayfarer/src/sfx.c's own sfx_play_swing()/sfx_play_pickup()/
// sfx_play_win() already computed and verified this same way:
//   jump (C6, 1046.5 Hz): period 1923 (0x783)
//   score (E6, 1318.5 Hz): period 1949 (0x79D)
//   win base (G4, 392.0 Hz, sweeps up from here): period 1714 (0x6B2)

void sfx_init(void) {
    NR52_REG = AUDENA_ON;
    NR51_REG = AUDTERM_1_LEFT | AUDTERM_1_RIGHT | AUDTERM_2_LEFT | AUDTERM_2_RIGHT
             | AUDTERM_4_LEFT | AUDTERM_4_RIGHT;
    NR50_REG = AUDVOL_VOL_LEFT(7) | AUDVOL_VOL_RIGHT(7);
}

// Channel 1 - short high blip, no sweep, ~59ms. Identical shape and
// note to wayfarer/src/sfx.c's own sfx_play_swing() (and, before it,
// prism/src/sfx.c's own sfx_play_select()) - a "you just did a routine
// action" blip, the same role a jump plays here.
void sfx_play_jump(void) {
    NR10_REG = 0;
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(49);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR13_REG = 0x83;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x07;
}

// Channel 2 - a different channel from sfx_play_jump()'s own channel 1
// so a barrel cleared mid-flight never steps on a jump blip still
// decaying. Same short-blip shape and exact note as wayfarer/src/
// sfx.c's own sfx_play_pickup() ("you got something" - a scored barrel
// is this game's closest equivalent), moved to channel 2's own
// registers (NR2x) rather than channel 1's.
void sfx_play_score(void) {
    NR21_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(49);
    NR22_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR23_REG = 0x9D;
    NR24_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x07;
}

// Channel 4 (noise) - a different channel and timbre from every tone
// above, so a barrel hit reads as genuinely distinct from routine
// move-feedback blips. Identical to wayfarer/src/sfx.c's own
// sfx_play_damage(): frequency = 262144 / (divisor * 2^shift) per
// pandocs, shift=5, divisor-code 2, 15-bit LFSR width (a hiss, not a
// metallic buzz): 262144/(2*32) = 4096 Hz.
void sfx_play_hit(void) {
    NR41_REG = AUDLEN_LENGTH(45);
    NR42_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR43_REG = (uint8_t)((5 << 4) | AUD4POLY_WIDTH_15BIT | 2);
    NR44_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON;
}

// Channel 1 again (never concurrent with sfx_play_jump() - main.c's
// own won flag stops player_update()/jumping entirely once this
// fires) - a rising hardware sweep, identical shape, sweep rate, and
// base note to wayfarer/src/sfx.c's own sfx_play_win() (itself the
// "held longer" triumphant variant of its sfx_play_hit()/prism's
// sfx_play_match()): period 1714 up by (period>>6) every 2/128s step,
// safely under the 2047 sweep-overflow ceiling across this note's own
// ~203ms life (length data 12) - verified safe there, unchanged here.
void sfx_play_win(void) {
    NR10_REG = AUD1SWEEP_UP | AUD1SWEEP_TIME(2) | AUD1SWEEP_LENGTH(6);
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(12);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(3));
    NR13_REG = 0xB2;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x06;
}

// Channel 4 (noise) again - a different, more dramatic decay from
// sfx_play_hit()'s own short burst, so a run genuinely ending reads as
// distinct from a routine hit. Identical to prism/src/sfx.c's and
// wayfarer/src/sfx.c's own sfx_play_gameover(): no length counter (the
// note isn't cut short by NR41's 250ms ceiling), so it's the volume
// envelope's own fade to 0 (15 steps at a 4/64s period, ~0.94s total)
// that ends it. shift=11, divisor-code=1 (divisor 1), 15-bit LFSR
// width: frequency = 262144 / (1*2048) = 128 Hz, a suitably low buzz -
// same formula and grounding sfx_play_hit()'s own comment already
// cites.
void sfx_play_gameover(void) {
    NR41_REG = AUDLEN_LENGTH(0);
    NR42_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(4));
    NR43_REG = (uint8_t)((11 << 4) | AUD4POLY_WIDTH_15BIT | 1);
    NR44_REG = AUDHIGH_RESTART;
}
