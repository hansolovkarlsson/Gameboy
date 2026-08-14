// Four one-shot sound effects, each a single write-and-restart to one
// channel's registers - real GB hardware envelope/sweep/length-counter
// decay does the rest, no per-frame service loop needed. Direct DMG/
// CGB sound-register pokes, same approach as the sibling prism/ and
// wayfarer/ projects' own sfx.c (no higher-level GBDK sound API). See
// sfx.c.

#ifndef ASCENT_SFX_H
#define ASCENT_SFX_H

void sfx_init(void);

void sfx_play_jump(void);      // the player leaves the ground - fires on every jump, cleared or not
void sfx_play_score(void);     // a barrel was successfully jumped over (100 points)
void sfx_play_hit(void);       // a barrel hit the player - triggers the respawn
void sfx_play_win(void);       // the goal flag was reached
void sfx_play_gameover(void);  // the last life was just spent

#endif
