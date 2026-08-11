// "Wayfarer" (working title) - a new, separate homebrew game from the
// sibling prism/ project (a match-3 puzzle game): a top-down
// action-adventure in the tradition of the original Zelda (1986) -
// free pixel-level movement around bordered rooms, viewed from above.
// See docs/GAMEBOY_ROADMAP.md's own entry for the whole milestone
// roadmap. This pass (Milestone 8) adds a "WAYFARER" / "PRESS START"
// title screen (title.h/title.c) before gameplay begins.
//
// See room.h/room.c for a room's own tile/palette data and per-side
// wall bounds, player.h/player.c for the directional sprite and
// movement/collision, world.h/world.c for the grid position and
// transition logic, sfx.h/sfx.c for the sword/hit/pickup/damage/win
// sound effects.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>

#include "sfx.h"
#include "title.h"
#include "world.h"

void main(void) {
    // Seeded now even though nothing's randomized yet - matches
    // prism/'s own initrand(DIV_REG)-as-first-line convention: the RNG
    // seed must be sampled before title_screen()'s own real,
    // player-paced wait-for-Start loop (an unbounded wall-clock delay),
    // the same reasoning prism/'s own Milestone 6b already established
    // for its own title screen.
    initrand(DIV_REG);

    sfx_init();
    title_screen(); // handles its own real-hardware-safe LCD-disable timing at both ends

    world_init(); // loads the room/player art and draws the starting room; also enables SHOW_SPRITES

    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        uint8_t joy = joypad();
        world_update(joy);
        vsync();
    }
}
