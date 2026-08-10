// Milestone 6c - real SRAM high score persistence. See
// docs/GAMEBOY_ROADMAP.md's Phase 10 entry for the full milestone
// roadmap. Milestone 6b (title screen) added the "PRISM" / "PRESS
// START" screen the player sees before gameplay begins
// (prism/src/title.h/title.c); this pass makes it also show the best
// score ever reached, persisted to real battery-backed cartridge RAM
// (prism/src/highscore.h/highscore.c - see prism/Makefile's
// -Wl-yt0x03/-Wm-ya1 flags) rather than something that resets every
// power cycle. See prism/src/sfx.h/sfx.c for the four one-shot SFX
// (select/revert/match/game-over) and their hook points below,
// prism/src/board.h/board.c for the grid state and match/gravity/
// refill logic, prism/src/cursor.h/cursor.c for the cursor sprite, and
// prism/src/hud.h/hud.c for the score/moves display.
//
// Selection is a second, small state machine alongside the cursor: `A`
// with no active selection selects the cursor's current cell (shown by
// the small filled-square marker sprite below, sprite slot 4 - distinct
// from both the gem diamonds and the cursor's corner brackets). `A`
// again on the same cell deselects. `A` while the cursor is on an
// *adjacent* cell attempts a swap (board_try_swap()) and always
// deselects afterward, whether the swap stuck or reverted - but only if
// moves_remaining > 0; once it hits 0, further swap attempts are
// ignored (cursor movement still works harmlessly). `A` while the
// cursor is on a non-adjacent cell just moves the selection there
// instead - forgiving UX, not an error. A committed (matching) swap
// costs one move; a reverted one costs nothing - you're not penalized
// for an invalid-looking attempt the game itself undoes. `Start` always
// restarts, mid-game or after game-over alike, no separate gating.
//
// See prism/README.md's "known emulator timing quirk" note: capturing
// a frame too soon after DISPLAY_ON (found in Milestone 2, re-confirmed
// stable in Milestones 3 and 4) can show a stale pre-write render;
// empirically confirmed safe from frame 10 onward for a static scene -
// scripted --input verification for this milestone captures with
// generous margin past the last input event for the same reason.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>

#include "board.h"
#include "cursor.h"
#include "gems.h"
#include "grid.h"
#include "highscore.h"
#include "hud.h"
#include "sfx.h"
#include "title.h"

// 20 is this build's real, shipped starting move budget - not a
// debug/testing value (see docs/GAMEBOY_ROADMAP.md's Phase 10 Milestone
// 5 entry for how the game-over transition was verified without
// scripting 20 real match-producing swaps blind).
#define STARTING_MOVES 20

static uint16_t score = 0;
static uint8_t moves_remaining = STARTING_MOVES;

// One 8x8 filled square, centered - the "selected cell" marker. Color
// index 0 is hardware-transparent for sprites unconditionally (same
// reasoning as cursor.c's own corner-bracket tile), so only the
// 4x4 center block needs a nonzero color index.
#define MARKER_TILE_ID 1 // cursor.c already owns tile ID 0
static const uint8_t marker_tile[16] = {
    0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0x3C, 0x3C,
    0x3C, 0x3C, 0x3C, 0x3C, 0x00, 0x00, 0x00, 0x00,
};

// A different OBJ palette (index 1) from the cursor's (index 0,
// cursor.c) - bright magenta/pink, distinct from both the cursor
// brackets and any of the five gem colors (gems.c).
static const palette_color_t marker_palette[4] = {
    RGB(0, 0, 0), RGB(20, 2, 20), RGB(26, 3, 26), RGB(31, 4, 31),
};

#define MARKER_SPRITE 4 // sprite slots 0-3 are cursor.c's corner brackets

static uint8_t selected = 0;
static uint8_t selected_x = 0;
static uint8_t selected_y = 0;

static void marker_init(void) {
    set_sprite_palette(1, 1, marker_palette);
    set_sprite_data(MARKER_TILE_ID, 1, marker_tile);
    set_sprite_tile(MARKER_SPRITE, MARKER_TILE_ID);
    set_sprite_prop(MARKER_SPRITE, S_PAL(1)); // CGB OBJ palette select (bits 0-2) - not S_PALETTE (0x10, the DMG-only OBP0/OBP1 bit)
    move_sprite(MARKER_SPRITE, 0, 0); // off-screen - move_sprite's own doc: Y=0 hides a sprite
}

// GBDK's move_sprite() takes screen position +8 (x) / +16 (y) - see
// cursor.c's own position_sprites() comment for the same convention.
static void marker_show(uint8_t gx, uint8_t gy) {
    uint8_t px = GRID_ORIGIN_X * 8 + gx * 16 + 4; // +4 to center the 8x8
    uint8_t py = GRID_ORIGIN_Y * 8 + gy * 16 + 4; // marker within the 16x16 cell
    move_sprite(MARKER_SPRITE, px + 8, py + 16);
}

static void marker_hide(void) {
    move_sprite(MARKER_SPRITE, 0, 0);
}

// Manhattan-adjacent (not diagonal) - a real match-3 swap is always
// with a horizontal or vertical neighbor, never diagonal.
static uint8_t is_adjacent(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    int8_t dx = (int8_t)x1 - (int8_t)x2;
    int8_t dy = (int8_t)y1 - (int8_t)y2;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx + dy) == 1;
}

static void handle_select(uint8_t pressed_a) {
    if (!pressed_a) return;

    uint8_t cx = cursor_get_x();
    uint8_t cy = cursor_get_y();

    if (!selected) {
        selected = 1;
        selected_x = cx;
        selected_y = cy;
        marker_show(cx, cy);
        sfx_play_select();
        return;
    }

    if (cx == selected_x && cy == selected_y) {
        selected = 0;
        marker_hide();
        return;
    }

    if (is_adjacent(selected_x, selected_y, cx, cy)) {
        if (moves_remaining > 0) {
            uint16_t cleared = board_try_swap(selected_x, selected_y, cx, cy);
            if (cleared > 0) {
                sfx_play_match();
                score += cleared * 10;
                highscore_maybe_update(score); // every scoring swap, not just game-over - see highscore.c
                moves_remaining--;
                hud_set_score(score);
                hud_set_moves(moves_remaining);
                if (moves_remaining == 0) sfx_play_gameover();
            } else {
                sfx_play_revert();
            }
        }
        selected = 0;
        marker_hide();
        return;
    }

    // Non-adjacent: move the selection to the new cell instead.
    selected_x = cx;
    selected_y = cy;
    marker_show(cx, cy);
}

// Always available, mid-game or after game-over alike - no separate
// gating on moves_remaining == 0.
static void restart_game(void) {
    score = 0;
    moves_remaining = STARTING_MOVES;
    selected = 0;
    marker_hide();
    board_init();
    hud_set_score(score);
    hud_set_moves(moves_remaining);
}

void main(void) {
    // initrand(DIV_REG) must stay the very first line here, before
    // title_screen()'s own real, player-paced wait-for-Start loop -
    // the RNG seed has to be sampled before any unbounded wall-clock
    // delay, or the random board would depend on how long the player
    // lingered on the title screen (see prism/src/title.c and
    // docs/GAMEBOY_ROADMAP.md's Phase 10 Milestone 6b entry).
    initrand(DIV_REG);

    sfx_init();
    highscore_init(); // must run before title_screen(), which displays it
    title_screen();

    set_bkg_palette(0, GEM_TYPE_COUNT, gem_palettes);
    set_bkg_data(0, GEM_TILE_COUNT, gem_tiles);
    hud_init();
    board_init();
    hud_set_score(score);
    hud_set_moves(moves_remaining);

    SHOW_BKG;
    DISPLAY_ON;

    cursor_init(); // also enables SHOW_SPRITES
    marker_init();

    // Seeded from the real current joypad state, not hardcoded 0 - if
    // the player is still physically holding Start (the same button
    // that just dismissed the title screen) when gameplay begins, a
    // hardcoded 0 would see that held Start as a *new* edge on the
    // very first iteration below and immediately call restart_game(),
    // silently consuming an extra rand() draw and regenerating the
    // board the player just saw appear.
    uint8_t prev_joy = joypad();
    while (1) {
        uint8_t joy = joypad();
        uint8_t pressed = (uint8_t)(joy & (uint8_t)~prev_joy);
        prev_joy = joy;

        cursor_update(joy);
        handle_select(pressed & J_A);
        if (pressed & J_START) restart_game();

        vsync();
    }
}
