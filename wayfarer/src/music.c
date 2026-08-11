// See music.h. A 32-sample triangle wave (ramps 0->15->0 across the
// wave channel's own 4-bit sample range) programmed into AUD3WAVE[16]
// once at init - a soft, mellow timbre, deliberately different in
// *character* from every sfx's own sharp duty-cycle pulse tone (the
// same "distinct timbre, not just distinct pitch" reasoning
// sfx_play_damage()'s own noise-channel choice already established),
// not just a different pitch.
//
// Channel 3's own frequency formula is genuinely different from
// channels 1/2's (sfx.c's own notes): freq = 65536 / (2048 - period),
// not 131072 - the wave channel advances its 32-step table at half the
// rate a pulse channel advances its 8-step duty cycle, so the same
// period value yields half the frequency. Each note's period below is
// period = round(2048 - 65536/freq), computed offline the same
// "generate programmatically from a real formula" way every sfx.c note
// already is:
//   C4 (261.63 Hz): 1798 (0x706)   D4 (293.66 Hz): 1825 (0x721)
//   E4 (329.63 Hz): 1849 (0x739)   F4 (349.23 Hz): 1860 (0x744)
//   G4 (392.00 Hz): 1881 (0x759)   A4 (440.00 Hz): 1899 (0x76B)
//   C5 (523.25 Hz): 1923 (0x783)
//
// An original 16-step phrase in C major (not a transcription of any
// existing game's real theme), quarter notes at ~120 BPM (30 frames/
// note at ~59.7 fps; two notes held for 60 frames as small "arrival"/
// cadence points), plus a trailing rest - 14 short steps + 2 held
// steps + 1 rest = (14*30)+(2*60)+30 = 540 frames, loops roughly
// every 9.0s:
//   C4 E4 G4 E4 F4 A4 G4 F4 E4 G4 C5(held) G4 F4 D4 E4(held) rest

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "music.h"

static const uint8_t wave_table[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
};

#define NOTE_C4 1798
#define NOTE_D4 1825
#define NOTE_E4 1849
#define NOTE_F4 1860
#define NOTE_G4 1881
#define NOTE_A4 1899
#define NOTE_C5 1923
#define REST 0 // real note periods are always far above 0 - a safe sentinel

typedef struct {
    uint16_t period;
    uint8_t frames;
} music_step_t;

#define STEP_COUNT 16
static const music_step_t steps[STEP_COUNT] = {
    { NOTE_C4, 30 }, { NOTE_E4, 30 }, { NOTE_G4, 30 }, { NOTE_E4, 30 },
    { NOTE_F4, 30 }, { NOTE_A4, 30 }, { NOTE_G4, 30 }, { NOTE_F4, 30 },
    { NOTE_E4, 30 }, { NOTE_G4, 30 }, { NOTE_C5, 60 }, { NOTE_G4, 30 },
    { NOTE_F4, 30 }, { NOTE_D4, 30 }, { NOTE_E4, 60 }, { REST,   30 },
};

static uint8_t step_index;
static uint8_t frames_left;

static void play_step(void) {
    uint16_t period = steps[step_index].period;
    if (period == REST) {
        NR30_REG = 0x00; // DAC off - silences immediately, no retrigger
    } else {
        NR30_REG = 0x80; // DAC on
        NR33_REG = (uint8_t)(period & 0xFF);
        // Restart (wave position resets to 0, a clean note start) but
        // deliberately no AUDHIGH_LENGTH_ON - this channel is driven
        // entirely by this sequencer's own frame countdown, not the
        // hardware length counter.
        NR34_REG = (uint8_t)(AUDHIGH_RESTART | ((period >> 8) & 0x07));
    }
    frames_left = steps[step_index].frames;
}

void music_init(void) {
    for (uint8_t i = 0; i < 16; i++) AUD3WAVE[i] = wave_table[i];
    NR32_REG = 0x60; // output level 3 (25%) - quiet enough that discrete sfx events (all at max envelope volume) stay individually distinguishable above the continuous background tone, confirmed empirically against this project's own analyze_sfx.py peak detector, not assumed from a plausible-sounding default

    step_index = 0;
    play_step();
}

void music_update(void) {
    frames_left--;
    if (frames_left == 0) {
        step_index++;
        if (step_index >= STEP_COUNT) step_index = 0;
        play_step();
    }
}

void music_stop(void) {
    NR30_REG = 0x00;
}
