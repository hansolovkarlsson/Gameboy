// Animated gem swap for Prism (Milestone 7,
// docs/GAMEBOY_ROADMAP.md's Phase 10 entry). Reuses gems.h's own
// diamond-quadrant bitmap and CGB palettes - loaded once more into
// sprite tile/OBJ-palette address space (see gb/gb.h's set_bkg_data()
// doc note: "Sprite Tiles 128-255 share the same memory region as
// Background Tiles 128-255" - every ID used here is well under 128, so
// no collision with the background gem/HUD tile IDs) - so a swap
// visibly slides the two gems across each other instead of the grid
// just instantly updating.

#ifndef PRISM_SWAPANIM_H
#define PRISM_SWAPANIM_H

#include <stdint.h>

// One-time setup: loads the 4 diamond-quadrant sprite tiles and 5 OBJ
// palettes (one per gem color, reusing gems.h's gem_tiles/gem_palettes
// values directly - the moving sprite is the exact same shape/color as
// the background gem it stands in for). Call once at startup, after
// cursor_init()/marker_init() (main.c) - those own sprite tile ID 0/1,
// OBJ palette 0/1, and sprite slots 0-4; this claims the next free
// ranges.
void swapanim_init(void);

// Blanks the background at both (from_x,from_y) and (to_x,to_y) (each
// a 2x2-tile gem cell - main.c always calls this with adjacent cells,
// though nothing here requires it), then slides two 16x16 sprite gems
// across each other over a fixed number of frames: one colored
// gem_from starting at (from_x,from_y) and ending at (to_x,to_y), the
// other colored gem_to starting at (to_x,to_y) and ending at
// (from_x,from_y). Blocking - calls vsync() itself each frame, the
// same shape title.c's own wait-for-Start loop already uses. Hides
// both sprites at the end but leaves the background still blanked at
// both cells - the caller must redraw afterward (board_try_swap()'s
// own internal board_redraw() on a commit, or an explicit
// board_redraw() call after a revert's slide-back call - see
// board.h).
void swapanim_play(uint8_t from_x, uint8_t from_y, uint8_t gem_from,
                    uint8_t to_x, uint8_t to_y, uint8_t gem_to);

#endif
