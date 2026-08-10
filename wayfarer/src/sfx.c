// See sfx.h. Direct DMG/CGB sound-register pokes, same approach and
// register-level grounding as the sibling prism/ project's own sfx.c
// (this emulator's own APU - src/apu.c - already fully implemented and
// verified, tests/test_apu.c and test_roms/droneboy).
//
// Channel 1/2/3's 11-bit "period" register (NRx3 low 8 bits + NRx4 low
// 3 bits) relates to real frequency via pandocs' own conversion:
// frequency = 131072 / (2048 - period), i.e. period = 2048 -
// 131072/frequency. Computed once offline (python3 -c
// "print(round(2048 - 131072/freq))"), same "generate programmatically
// from a real formula" discipline as prism/'s own sfx.c:
//   swing (C6, 1046.5 Hz): period 1923 (0x783) - identical note to
//     prism/'s own sfx_play_select(), same short-blip shape
//   hit/win base (G4, 392.0 Hz, sweeps up from here): period 1714 (0x6B2) -
//     identical note to prism/'s own sfx_play_match()
//   pickup (E6, 1318.5 Hz): period 1949 (0x79D)

#include <gb/gb.h>
#include <stdint.h>

void sfx_init(void) {
    NR52_REG = AUDENA_ON;
    NR51_REG = AUDTERM_1_LEFT | AUDTERM_1_RIGHT | AUDTERM_4_LEFT | AUDTERM_4_RIGHT;
    NR50_REG = AUDVOL_VOL_LEFT(7) | AUDVOL_VOL_RIGHT(7);
}

// Short high blip, no sweep, ~59ms - fires on every swing attempt, hit
// or miss.
void sfx_play_swing(void) {
    NR10_REG = 0;
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(49);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR13_REG = 0x83;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x07;
}

// Rising pitch via a real hardware sweep (AUD1SWEEP_UP, period
// increases): period 1714 up by (period>>6) every 2/128s step - within
// this note's ~148ms life (length data 26) it rises to roughly
// 1714+9*26=~1948, still under the 2047 sweep-overflow ceiling, same
// verified-safe numbers as prism/'s own sfx_play_match().
void sfx_play_hit(void) {
    NR10_REG = AUD1SWEEP_UP | AUD1SWEEP_TIME(2) | AUD1SWEEP_LENGTH(6);
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(26);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(3));
    NR13_REG = 0xB2;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x06;
}

// Same short-blip shape as sfx_play_swing(), a different pitch (E6) so
// the two read as distinct events. Shared by both the heart pickup and
// the key - a real, deliberate scope decision: both are "you got
// something," not worth a second, extremely similar trigger to tell
// them apart audibly.
void sfx_play_pickup(void) {
    NR10_REG = 0;
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(49);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR13_REG = 0x9D;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x07;
}

// A different channel and timbre from the four channel-1 tones above -
// channel 4 (noise), so taking damage reads as genuinely distinct from
// any move-feedback blip. Higher-pitched and much shorter than
// prism/'s own sfx_play_gameover() (shift=5 vs. its 11, length counter
// left ON for a firm ~74ms burst rather than a long dramatic fade) -
// this is a routine "you got hit," not a session-ending event.
// frequency = 262144 / (divisor * 2^shift) per pandocs; divisor-code 2
// (divisor 2), shift 5, 15-bit LFSR width (a hiss, not a metallic
// buzz): 262144/(2*32) = 4096 Hz.
void sfx_play_damage(void) {
    NR41_REG = AUDLEN_LENGTH(45);
    NR42_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR43_REG = (uint8_t)((5 << 4) | AUD4POLY_WIDTH_15BIT | 2);
    NR44_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON;
}

// The same rising-sweep shape and base note as sfx_play_hit(), just
// held longer (~203ms, length data 12 vs. hit's 26) - a triumphant
// variant, not a fully distinct sound, reflecting that a win is
// thematically "a bigger hit." Same sweep rate as sfx_play_hit()
// (verified safe there); over the longer duration the rise reaches
// the overflow ceiling right around the note's own natural end
// (1714+13*26=~2052 at ~203ms, matching the note's own length) rather
// than cutting it off early.
void sfx_play_win(void) {
    NR10_REG = AUD1SWEEP_UP | AUD1SWEEP_TIME(2) | AUD1SWEEP_LENGTH(6);
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(12);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(3));
    NR13_REG = 0xB2;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x06;
}
