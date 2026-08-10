// Real game state for Prism's 6x6 grid (Milestone 4,
// docs/GAMEBOY_ROADMAP.md's Phase 10 entry) - the match-3 mechanic.
// Replaces Milestone 2's formula-driven grid (gem_type = (row+col)%5)
// with a real grid[][] in WRAM, since match detection needs to know
// what's actually where, not just what a fixed pattern would put there.

#ifndef PRISM_BOARD_H
#define PRISM_BOARD_H

#include <stdint.h>

// Fills the background with the blank tile, fills the grid with a
// random arrangement that contains no pre-existing 3-in-a-row (a fresh
// board should never hand the player a free match), and draws it. Call
// once at startup, after set_bkg_data/set_bkg_palette have loaded the
// gem tile/palette data (gems.h) and initrand() has been called.
void board_init(void);

// Attempts to swap grid cells (x1,y1) and (x2,y2) (expected adjacent,
// though this function doesn't itself enforce that - main.c's
// selection logic already only calls it for adjacent cells). If the
// swap creates no match anywhere on the grid, reverts it immediately
// and returns 0 - the grid is visually unchanged. If it creates at
// least one match, commits the swap, then resolves clear -> gravity ->
// refill, re-checking for new matches after each refill (a refill cell
// creating a new match is a real cascade/combo, not a bug to avoid -
// unlike the initial board_init() fill) until a pass finds none,
// redraws once at the end, and returns the *total* number of gems
// cleared across every pass (Milestone 5's score needs this - a
// multi-step cascade clears more than just the first match). No
// animation - the player sees the pre-swap and fully-resolved
// post-cascade states, not the steps between (deferred to later
// polish, Milestone 6).
uint16_t board_try_swap(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

#endif
