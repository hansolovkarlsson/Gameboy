// "Ascent" (working title) - a third, separate homebrew game from the
// sibling prism/ (match-3 puzzle) and wayfarer/ (top-down action-
// adventure) projects: a single-screen platformer in the tradition of
// the original Donkey Kong (1981) - stacked girders and zigzagging
// ladders, climbed from bottom to top. See docs/GAMEBOY_ROADMAP.md's
// own entry for the full milestone roadmap. This pass (Milestone 1)
// establishes the core physics on one static screen: gravity, platform
// standing, and ladder climbing. No jump, no barrels, no goal, no HUD
// yet - see stage.h/stage.c for the screen's tile art/layout,
// player.h/player.c for the physics.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>

#include "stage.h"
#include "player.h"

void main(void) {
    initrand(DIV_REG);

    stage_init();
    player_init();

    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        uint8_t joy = joypad();
        player_update(joy);
        vsync();
    }
}
