// A single, continuously looping background theme on channel 3 (the
// wave channel) - entirely unused by sfx.c's own one-shot swing/hit/
// pickup/damage/win/brute-hit/block sounds (channels 1, 2, 4), so
// music and any sfx mix in hardware for free, no ducking needed. See
// music.c for the wave shape and the melody itself.

#ifndef WAYFARER_MUSIC_H
#define WAYFARER_MUSIC_H

// Programs the wave RAM and output level once, and resets the
// sequencer to the first note - called once per session (world.c's
// reset_world(), shared by a real first boot and Milestone 11's own
// restart-from-the-win-screen, so a restart starts the theme fresh
// too, consistent with every other module's reset-on-restart
// behavior).
void music_init(void);

// Advances the sequencer by one frame - call unconditionally every
// frame gameplay is live. A no-op in wall-clock terms most frames;
// only writes new note registers on the exact frame a step's own
// duration expires.
void music_update(void);

// Silences channel 3 (DAC off) - called once, the moment the player
// wins, so the loop doesn't keep playing under the win screen/jingle.
void music_stop(void);

#endif
