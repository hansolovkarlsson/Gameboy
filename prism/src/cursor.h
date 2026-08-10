// Cursor sprite + input handling for Prism's 6x6 grid (Milestone 3,
// docs/GAMEBOY_ROADMAP.md's Phase 10 entry). A hardware sprite (OBJ)
// overlay, not a background-tile trick - the standard approach for
// something that moves independently of the tile grid underneath it.
// No A-button/select behavior yet - that's Milestone 4, once it has
// match-detection-adjacent state to make sense of a selection.

#ifndef PRISM_CURSOR_H
#define PRISM_CURSOR_H

#include <stdint.h>

// One-time setup: loads the corner-bracket sprite tile and OBJ palette,
// assigns the four corner sprites, and positions them at the initial
// cursor cell (0,0). Call once, after the grid itself has been drawn.
void cursor_init(void);

// Edge-triggered against the previous call's reading of `joy` - the
// D-pad moves the cursor by at most one grid cell on the frame a
// direction transitions from not-pressed to pressed, not every frame
// it's held down - and repositions the four corner sprites to match.
// Call once per real frame, passing that frame's joypad() reading -
// Milestone 4's selection/swap logic (main.c) needs the same reading,
// so it's read once by the caller and shared, not read again here.
void cursor_update(uint8_t joy);

// Current cursor cell, 0 to GRID_SIZE-1 on each axis - Milestone 4's
// selection/swap logic (main.c) needs to know which cell the cursor is
// over.
uint8_t cursor_get_x(void);
uint8_t cursor_get_y(void);

#endif
