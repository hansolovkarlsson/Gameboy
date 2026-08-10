// See sfx.h. Four one-shot triggers, each a single write-and-restart to
// one channel's registers - real GB hardware envelope/sweep/length-
// counter decay does the rest, no per-frame service loop needed. See
// docs/GAMEBOY_ROADMAP.md's Phase 10 Milestone 6a entry for the full
// design writeup.

#include <gb/gb.h>
#include <stdint.h>

// Channel 1/2/3's 11-bit "period" register (NRx3 low 8 bits + NRx4 low
// 3 bits) relates to real frequency via pandocs' own conversion:
// frequency = 131072 / (2048 - period), i.e. period = 2048 -
// 131072/frequency. Computed once offline (python3 -c
// "print(round(2048 - 131072/freq))") rather than hand-derived - same
// "generate programmatically from a real formula" discipline as the
// digit/gem tile bitmaps:
//   select (C6, 1046.5 Hz): period 1923 (0x783)
//   revert base (D5, 587.33 Hz, sweeps down from here): period 1825 (0x721)
//   match base (G4, 392.0 Hz, sweeps up from here): period 1714 (0x6B2)

void sfx_init(void) {
    NR52_REG = AUDENA_ON;
    NR51_REG = AUDTERM_1_LEFT | AUDTERM_1_RIGHT | AUDTERM_4_LEFT | AUDTERM_4_RIGHT;
    NR50_REG = AUDVOL_VOL_LEFT(7) | AUDVOL_VOL_RIGHT(7);
}

// Short high blip, no sweep, ~59ms (length data 49 -> (64-49)/256s).
void sfx_play_select(void) {
    NR10_REG = 0;
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(49);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR13_REG = 0x83;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x07;
}

// Falling pitch via a real hardware sweep in AUD1SWEEP_DOWN (period-
// decreases) mode - period 1825 down by (period>>3) every 2/128s step,
// well clear of underflow within this note's ~102ms life (length data
// 38). Sweep direction is written once at trigger and never changed
// before the note ends - this project's own APU history (see
// docs/GAMEBOY_ROADMAP.md's "sweep negate-disable quirk" fix) found
// real hardware disables the channel if a negate-mode sweep event fires
// after the negate bit was toggled off mid-note; not touching NR10
// again here avoids that path entirely, not just working around it by
// luck.
void sfx_play_revert(void) {
    NR10_REG = AUD1SWEEP_DOWN | AUD1SWEEP_TIME(2) | AUD1SWEEP_LENGTH(3);
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(38);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(2));
    NR13_REG = 0x21;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x07;
}

// Mirror image of sfx_play_revert(): rising pitch via AUD1SWEEP_UP
// (period increases). Period 1714 up by (period>>6) every 2/128s step -
// within this note's ~148ms life (length data 26) it rises to roughly
// 1714 + 9*26 =~ 1948, still under the 2047 sweep-overflow ceiling, so
// the note plays its full rising warble rather than getting cut off by
// an overflow-disable.
void sfx_play_match(void) {
    NR10_REG = AUD1SWEEP_UP | AUD1SWEEP_TIME(2) | AUD1SWEEP_LENGTH(6);
    NR11_REG = AUDLEN_DUTY_50 | AUDLEN_LENGTH(26);
    NR12_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(3));
    NR13_REG = 0xB2;
    NR14_REG = AUDHIGH_RESTART | AUDHIGH_LENGTH_ON | 0x06;
}

// A different channel and timbre, not just a different pitch, from the
// three channel-1 tones above - channel 4 (noise), so a "session over"
// event reads as genuinely distinct from another move-feedback blip.
// Noise has no frequency register - NR43's clock-shift/divisor fields
// set its pitch instead, per pandocs: frequency = 262144 /
// (divisor * 2^shift), where divisor is 0.5 for divisor-code 0 or
// divisor-code itself otherwise. shift=11, divisor-code=1 (divisor 1),
// 15-bit LFSR width (AUD4POLY_WIDTH_15BIT, a hiss rather than a
// metallic buzz) gives 262144/(1*2048) = 128 Hz, a suitably low buzz.
// The length counter is left disabled (no AUDHIGH_LENGTH_ON) so the
// note isn't cut short by NR41's 250ms ceiling - it's the volume
// envelope's own fade to 0 (15 steps at a 4/64s period, ~0.94s total)
// that ends it, a deliberately longer, more dramatic decay than the
// three short move-feedback blips above.
void sfx_play_gameover(void) {
    NR41_REG = AUDLEN_LENGTH(0);
    NR42_REG = (uint8_t)(AUDENV_VOL(15) | AUDENV_DOWN | AUDENV_LENGTH(4));
    NR43_REG = (uint8_t)((11 << 4) | AUD4POLY_WIDTH_15BIT | 1);
    NR44_REG = AUDHIGH_RESTART;
}
