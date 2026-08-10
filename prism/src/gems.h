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

#endif
