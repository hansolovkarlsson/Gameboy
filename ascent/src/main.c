// "Ascent" (working title) - a third, separate homebrew game from the
// sibling prism/ (match-3 puzzle) and wayfarer/ (top-down action-
// adventure) projects: a single-screen platformer in the tradition of
// the original Donkey Kong (1981) - stacked girders and zigzagging
// ladders, climbed from bottom to top. See docs/GAMEBOY_ROADMAP.md's
// own entry for the full milestone roadmap. Milestone 1 established the
// core physics on one static screen: gravity, platform standing, and
// ladder climbing. This pass (Milestone 2) adds a jump and rolling
// barrels that respawn the player at the ground on contact - see
// player.h/player.c for the jump, barrel.h/barrel.c for the barrels.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>

#include "stage.h"
#include "player.h"
#include "barrel.h"

void main(void) {
    initrand(DIV_REG);

    stage_init();
    player_init();
    barrel_init();

    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        uint8_t joy = joypad();
        player_update(joy);
        barrel_update();
        if (barrel_check_hit(player_get_x(), player_get_y())) {
            player_respawn();
        }
        vsync();
    }
}
