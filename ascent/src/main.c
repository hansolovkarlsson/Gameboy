// "Ascent" (working title) - a third, separate homebrew game from the
// sibling prism/ (match-3 puzzle) and wayfarer/ (top-down action-
// adventure) projects: a single-screen platformer in the tradition of
// the original Donkey Kong (1981) - stacked girders and zigzagging
// ladders, climbed from bottom to top. See docs/GAMEBOY_ROADMAP.md's
// own entry for the full milestone roadmap. Milestones 1-3 established
// the core physics on one static screen: gravity, platform standing,
// ladder climbing (both directions), a jump, and rolling barrels that
// respawn the player at the ground on contact. Milestone 4 added a win
// condition - a flag on the top platform (goal.h/goal.c) that ends the
// run with a "WIN" screen (win.h/win.c) once reached. This pass
// (Milestone 5) adds the one way back out of that screen: a Start
// press restarts the whole game, the same escape
// wayfarer/src/world.c's own win screen already gives - re-running the
// exact same init sequence real boot uses, since every module's own
// _init() is already a full, idempotent reset (confirmed by reading
// each one - none of them carry state across calls that a fresh call
// wouldn't itself overwrite).

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
    uint8_t prev_joy = 0;
    while (1) {
        uint8_t joy = joypad();
        // Edge-detected (not held) so a Start press restarts exactly
        // once, not every frame it stays held - same pattern
        // wayfarer/src/world.c's own won-state Start handling uses.
        // Updated every frame regardless of branch so the very first
        // frame after winning doesn't see a stale press from earlier
        // play (Start isn't read at all outside this branch, but
        // keeping prev_joy current avoids relying on that).
        uint8_t pressed = (uint8_t)(joy & (uint8_t)~prev_joy);
        prev_joy = joy;

        if (won) {
            if (pressed & J_START) {
                // Re-runs the exact same setup real boot uses - every
                // module's own _init() already fully resets its own
                // state (position, sprites, barrel array, spawn timer),
                // so nothing from the finished run carries forward.
                wait_vbl_done();
                DISPLAY_OFF;

                stage_init();
                player_init();
                barrel_init();
                goal_init();

                SHOW_BKG;
                DISPLAY_ON;

                won = 0;
            }
        } else {
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
