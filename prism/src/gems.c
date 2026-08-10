// See gems.h. Diamond bitmap generated from a 16x16 Manhattan-distance
// diamond (|dx|+|dy| <= 7.5 from center) split into four 8x8 GB 2bpp
// tiles - not hand-drawn pixel by pixel, computed and converted to GB's
// planar tile format (low bitplane byte, high bitplane byte, per row)
// by a one-off local script, then verified correct by actually
// rendering it through this project's own emulator (see
// docs/GAMEBOY_ROADMAP.md's Phase 10 Milestone 2 entry) before being
// committed here - not hand-typed hex guessed to look right.

#include "gems.h"

const uint8_t gem_tiles[GEM_TILE_COUNT * 16] = {
    // Tile 0: top-left quadrant
    0x00, 0x00, 0x01, 0x01, 0x03, 0x03, 0x07, 0x07,
    0x0F, 0x0F, 0x1F, 0x1F, 0x3F, 0x3F, 0x7F, 0x7F,
    // Tile 1: top-right quadrant
    0x00, 0x00, 0x80, 0x80, 0xC0, 0xC0, 0xE0, 0xE0,
    0xF0, 0xF0, 0xF8, 0xF8, 0xFC, 0xFC, 0xFE, 0xFE,
    // Tile 2: bottom-left quadrant
    0x7F, 0x7F, 0x3F, 0x3F, 0x1F, 0x1F, 0x0F, 0x0F,
    0x07, 0x07, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00,
    // Tile 3: bottom-right quadrant
    0xFE, 0xFE, 0xFC, 0xFC, 0xF8, 0xF8, 0xF0, 0xF0,
    0xE0, 0xE0, 0xC0, 0xC0, 0x80, 0x80, 0x00, 0x00,
    // Tile 4: solid blank (color index 0 everywhere)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Shared board-background tone (dark navy) used as color 0 in every
// palette below.
#define BOARD_BG RGB(2, 2, 5)

const palette_color_t gem_palettes[GEM_TYPE_COUNT * 4] = {
    // 0: Red
    BOARD_BG, RGB(16, 2, 2), RGB(24, 3, 3), RGB(31, 4, 4),
    // 1: Green
    BOARD_BG, RGB(2, 14, 2), RGB(3, 21, 3), RGB(4, 28, 4),
    // 2: Blue
    BOARD_BG, RGB(3, 5, 16), RGB(4, 7, 24), RGB(6, 10, 31),
    // 3: Yellow
    BOARD_BG, RGB(16, 14, 1), RGB(24, 21, 1), RGB(31, 28, 2),
    // 4: Purple
    BOARD_BG, RGB(12, 2, 14), RGB(18, 3, 21), RGB(24, 4, 28),
};

// Medium (K=10) and small (K=12) diamond quadrants for the clear
// animation - same Manhattan-distance-formula generation and GB 2bpp
// planar layout as gem_tiles above (low bitplane byte, high bitplane
// byte, per row - both identical for this 2-color shape), just a
// smaller shape each time. See gems.h's own note on the K threshold
// and the flip-based single-tile-per-stage reuse technique.
const uint8_t gem_shrink_tiles[GEM_SHRINK_TILE_COUNT * 16] = {
    // Tile 36: medium quadrant (K=10)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x03, 0x03, 0x07, 0x07, 0x0F, 0x0F, 0x1F, 0x1F,
    // Tile 37: small quadrant (K=12)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x03, 0x03, 0x07, 0x07,
};

// A bright near-white flash - all four entries close to white so the
// flash reads the same regardless of which color index the diamond
// bitmap's 2 colors (0=background,3=fill) end up sampling; color 0
// isn't quite pure white to avoid the flash looking like a solid
// square rather than a lit diamond against a barely-dimmer surround.
const palette_color_t flash_palette[4] = {
    RGB(28, 28, 30), RGB(31, 31, 31), RGB(31, 31, 31), RGB(31, 31, 31),
};
