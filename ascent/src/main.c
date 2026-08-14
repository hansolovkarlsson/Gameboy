// "Ascent" (working title) - a third, separate homebrew game from the
// sibling prism/ (match-3 puzzle) and wayfarer/ (top-down action-
// adventure) projects: a single-screen platformer in the tradition of
// the original Donkey Kong (1981) - stacked girders and zigzagging
// ladders, climbed from bottom to top. See docs/GAMEBOY_ROADMAP.md's
// own entry for the full milestone roadmap. Milestones 1-3 established
// the core physics on one static screen: gravity, platform standing,
// ladder climbing (both directions), a jump, and rolling barrels that
// respawn the player at the ground on contact. This pass (Milestone 4)
// adds a real win condition - a flag on the top platform (goal.h/
// goal.c) that ends the run with a one-shot "WIN" screen (win.h/
// win.c) once reached.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>

#include "stage.h"
#include "player.h"
#include "barrel.h"
#include "goal.h"
#include "win.h"

void main(void) {
    initrand(DIV_REG);

    stage_init();
    player_init();
    barrel_init();
    goal_init();

    SHOW_BKG;
    DISPLAY_ON;

    uint8_t won = 0;
    while (1) {
        uint8_t joy = joypad();
        if (!won) {
            player_update(joy);
            barrel_update();
            if (barrel_check_hit(player_get_x(), player_get_y())) {
                player_respawn();
            }
            if (goal_check_reached(player_get_x(), player_get_y())) {
                won = 1;
                win_play();
            }
        }
        vsync();
    }
}
