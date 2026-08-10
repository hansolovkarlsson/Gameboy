#ifndef PRISM_SFX_H
#define PRISM_SFX_H

// Four one-shot sound effects, each a single register-poke trigger on
// real GB sound hardware (channel 1 square+sweep for select/revert/
// match, channel 4 noise for game-over) - no per-frame service loop,
// the hardware's own envelope/sweep/length-counter decay handles the
// rest. See docs/GAMEBOY_ROADMAP.md's Phase 10 Milestone 6a entry for
// the exact register values and how they were derived.
void sfx_init(void);
void sfx_play_select(void);
void sfx_play_revert(void);
void sfx_play_match(void);
void sfx_play_gameover(void);

#endif
