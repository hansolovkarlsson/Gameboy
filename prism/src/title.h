#ifndef PRISM_TITLE_H
#define PRISM_TITLE_H

// Shows Prism's title screen and blocks until the player presses
// Start. Must be called after initrand(DIV_REG) (see title.c) so its
// own real, player-paced wait doesn't affect the deterministic RNG
// seed the rest of the game depends on.
void title_screen(void);

#endif
