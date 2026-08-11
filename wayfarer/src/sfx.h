// Seven one-shot sound effects, each a single write-and-restart to one
// channel's registers - real GB hardware envelope/sweep/length-counter
// decay does the rest, no per-frame service loop needed. Direct DMG/
// CGB sound-register pokes, same approach as the sibling prism/
// project's own sfx.c (no higher-level GBDK sound API). See sfx.c.

#ifndef WAYFARER_SFX_H
#define WAYFARER_SFX_H

void sfx_init(void);

void sfx_play_swing(void);      // the sword swing itself - fires on every attempt, hit or miss
void sfx_play_hit(void);        // the sword connecting with the (one-hit) enemy, or the brute's second/lethal hit
void sfx_play_pickup(void);     // the heart pickup, key, sword, or shield collected
void sfx_play_damage(void);     // the player actually took damage (not a no-op invincible graze)
void sfx_play_win(void);        // the win condition triggered
void sfx_play_brute_hit(void);  // the brute's own first, non-lethal hit
void sfx_play_block(void);      // the shield successfully blocked a contact hit

#endif
