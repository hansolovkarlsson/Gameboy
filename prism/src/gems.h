// Gem tile bitmap and CGB palette data for Prism's 6x6 grid (Milestone
// 2, docs/GAMEBOY_ROADMAP.md's Phase 10 entry). Split out from main.c
// the same way GBDK's own colorbar.c example splits its generated tile/
// map data into bar_c.c/bar_m.c - keeps main.c focused on game setup,
// not art data.

#ifndef PRISM_GEMS_H
#define PRISM_GEMS_H

#include <gb/cgb.h>
#include <stdint.h>

// One shared 2x2-hardware-tile (16x16px) diamond icon, recolored per
// gem type via CGB background palette rather than having a distinct
// shape per type - see prism/README.md's Milestone 2 note. Tile IDs
// 0-3 are the diamond's four quadrants (top-left/top-right/bottom-left/
// bottom-right); tile ID 4 is a solid blank (every pixel color index
// 0) used to fill the background *outside* the grid - the whole 32x32
// background tile map defaults to whatever's in VRAM already (tile ID
// 0, i.e. the diamond's top-left quadrant) if never explicitly written,
// which without this tile would repeat across the entire screen.
#define GEM_TILE_COUNT 5
extern const uint8_t gem_tiles[GEM_TILE_COUNT * 16];
#define BLANK_TILE_ID 4

// Two smaller diamond quadrants for the matched-gem clear animation
// (Milestone 8, board.c's play_clear_animation()) - same
// Manhattan-distance formula as gem_tiles above, just with the "lit if
// x+y >= K" threshold raised from the original K=8 to K=10 (medium,
// ~60% linear size) and K=12 (small, ~30%), shrinking the diamond
// toward the icon's center rather than a new shape. Only one quadrant
// each (not 4) - board.c reuses it for all four corners via the CGB
// background attribute byte's own flip bits (gb/hardware.h's
// BKGF_XFLIP/BKGF_YFLIP), the same "one tile, four flipped corners"
// technique swapanim.c already uses on the sprite side. Tile IDs
// 36-37 - the next free BG range after title.c's digit tiles (26-35).
#define GEM_SHRINK_TILE_BASE 36
#define GEM_SHRINK_TILE_COUNT 2
extern const uint8_t gem_shrink_tiles[GEM_SHRINK_TILE_COUNT * 16];

// Five CGB background palettes (one per gem color), each 4 RGB555
// entries - color 0 is the same neutral board-background tone across
// all five (so gaps between gem icons read as one consistent
// background, not five different shades); colors 1-3 are shades of
// that palette's own hue. Only color 3 is actually used by the
// diamond bitmap above this milestone (a 2-color-only shape - see
// prism/README.md); 1-2 are defined now for later use rather than left
// meaningless.
#define GEM_TYPE_COUNT 5
extern const palette_color_t gem_palettes[GEM_TYPE_COUNT * 4];

// A bright near-white CGB BG palette, used only for the clear
// animation's one-frame flash stage (board.c's play_clear_animation())
// - applied as an attribute-only change against the *existing*
// full-size diamond tiles (0-3), so the flash itself needs no new tile
// data. Index 6: gems.c owns 0-4, title.c owns 5.
#define FLASH_PALETTE 6
extern const palette_color_t flash_palette[4];

#endif
