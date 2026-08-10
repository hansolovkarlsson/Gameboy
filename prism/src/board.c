// See board.h.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>

#include "board.h"
#include "gems.h"
#include "grid.h"

static uint8_t grid[GRID_SIZE][GRID_SIZE];
static uint8_t map_tiles[GRID_TILE_W * GRID_TILE_H];
static uint8_t map_attrs[GRID_TILE_W * GRID_TILE_H];

// Covers the *entire* 32x32 background tile map with a solid blank
// tile - the map otherwise defaults to whatever's in VRAM already
// (tile ID 0, i.e. the diamond's own top-left quadrant, repeated
// across the whole screen) if never explicitly written. See
// prism/README.md's "known emulator timing quirk" note, found via this
// exact code in Milestone 2.
//
// Also resets every cell's CGB attribute byte (palette index) to 0 -
// a real bug found via Milestone 6c: title_screen() (title.c) is the
// first code that ever sets non-zero attributes *outside* the 12x12
// grid region before this runs (its own "HIGH <score>" line, row 15,
// falls below the grid's own rows 3-14, which board_redraw() below
// re-attributes correctly but never touches anything past). Without
// this, those cells kept title.c's dark-navy TITLE_PALETTE background
// color instead of picking up a neutral one, showing as a visibly
// darker patch under the grid once gameplay started.
static void fill_screen_blank(void) {
    uint8_t blank_row[32];
    uint8_t blank_attrs[32];
    for (uint8_t i = 0; i < 32; i++) {
        blank_row[i] = BLANK_TILE_ID;
        blank_attrs[i] = 0;
    }
    for (uint8_t y = 0; y < 32; y++) {
        set_bkg_tiles(0, y, 32, 1, blank_row);
        set_bkg_attributes(0, y, 32, 1, blank_attrs);
    }
}

// Rebuilds the 12x12 tile-ID/attribute maps from grid[][] and draws
// them - same shape as Milestone 2's old build_grid(), just reading
// real state now instead of a fixed (row+col)%5 formula.
void board_redraw(void) {
    for (uint8_t gy = 0; gy < GRID_SIZE; gy++) {
        for (uint8_t gx = 0; gx < GRID_SIZE; gx++) {
            uint8_t gem_type = grid[gy][gx];
            uint8_t tx = gx * 2;
            uint8_t ty = gy * 2;

            map_tiles[ty * GRID_TILE_W + tx] = 0;             // top-left
            map_tiles[ty * GRID_TILE_W + tx + 1] = 1;          // top-right
            map_tiles[(ty + 1) * GRID_TILE_W + tx] = 2;        // bottom-left
            map_tiles[(ty + 1) * GRID_TILE_W + tx + 1] = 3;    // bottom-right

            map_attrs[ty * GRID_TILE_W + tx] = gem_type;
            map_attrs[ty * GRID_TILE_W + tx + 1] = gem_type;
            map_attrs[(ty + 1) * GRID_TILE_W + tx] = gem_type;
            map_attrs[(ty + 1) * GRID_TILE_W + tx + 1] = gem_type;
        }
    }
    set_bkg_tiles(GRID_ORIGIN_X, GRID_ORIGIN_Y, GRID_TILE_W, GRID_TILE_H, map_tiles);
    set_bkg_attributes(GRID_ORIGIN_X, GRID_ORIGIN_Y, GRID_TILE_W, GRID_TILE_H, map_attrs);
}

// Used only by board_init()'s fill: does placing gem_type at (x,y)
// complete an immediate 3-in-a-row? Only the already-placed left/
// upward neighbors need checking - cells to the right/below aren't
// filled yet (row-major fill order).
static uint8_t creates_match_at(uint8_t x, uint8_t y) {
    uint8_t v = grid[y][x];
    if (x >= 2 && grid[y][x - 1] == v && grid[y][x - 2] == v) return 1;
    if (y >= 2 && grid[y - 1][x] == v && grid[y - 2][x] == v) return 1;
    return 0;
}

// Scans every row and column for runs of 3+ identical adjacent values,
// marking to_clear accordingly (a cell can be marked by both a row-run
// and a column-run - to_clear stays boolean per cell, but the returned
// count below only counts each cell once regardless). Returns the
// number of distinct cells marked (0 = no match) - Milestone 5's score
// needs to know how many gems actually cleared, not just whether any
// did.
static uint8_t find_matches(uint8_t to_clear[GRID_SIZE][GRID_SIZE]) {
    for (uint8_t y = 0; y < GRID_SIZE; y++) {
        for (uint8_t x = 0; x < GRID_SIZE; x++) to_clear[y][x] = 0;
    }

    for (uint8_t y = 0; y < GRID_SIZE; y++) {
        uint8_t run_start = 0;
        for (uint8_t x = 1; x <= GRID_SIZE; x++) {
            if (x == GRID_SIZE || grid[y][x] != grid[y][run_start]) {
                if (x - run_start >= 3) {
                    for (uint8_t k = run_start; k < x; k++) to_clear[y][k] = 1;
                }
                run_start = x;
            }
        }
    }
    for (uint8_t x = 0; x < GRID_SIZE; x++) {
        uint8_t run_start = 0;
        for (uint8_t y = 1; y <= GRID_SIZE; y++) {
            if (y == GRID_SIZE || grid[y][x] != grid[run_start][x]) {
                if (y - run_start >= 3) {
                    for (uint8_t k = run_start; k < y; k++) to_clear[k][x] = 1;
                }
                run_start = y;
            }
        }
    }

    uint8_t count = 0;
    for (uint8_t y = 0; y < GRID_SIZE; y++) {
        for (uint8_t x = 0; x < GRID_SIZE; x++) count += to_clear[y][x];
    }
    return count;
}

// Per column: compact non-cleared cells downward (gravity), then
// refill the newly-empty top cells with fresh random values - no
// match-avoidance here, unlike board_init()'s fill, since a refill
// cell creating a new match is exactly what a cascade/combo is.
// Loop counters are int8_t, not uint8_t - this naturally decrements
// past 0, which wraps to 255 on an unsigned counter.
static void collapse_and_refill(uint8_t to_clear[GRID_SIZE][GRID_SIZE]) {
    for (uint8_t x = 0; x < GRID_SIZE; x++) {
        int8_t write_y = GRID_SIZE - 1;
        for (int8_t y = GRID_SIZE - 1; y >= 0; y--) {
            if (!to_clear[y][x]) {
                grid[write_y][x] = grid[y][x];
                write_y--;
            }
        }
        while (write_y >= 0) {
            grid[write_y][x] = rand() % GEM_TYPE_COUNT;
            write_y--;
        }
    }
}

uint8_t board_peek(uint8_t x, uint8_t y) {
    return grid[y][x];
}

void board_init(void) {
    fill_screen_blank();

    for (uint8_t y = 0; y < GRID_SIZE; y++) {
        for (uint8_t x = 0; x < GRID_SIZE; x++) {
            do {
                grid[y][x] = rand() % GEM_TYPE_COUNT;
            } while (creates_match_at(x, y));
        }
    }

    board_redraw();
}

uint16_t board_try_swap(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    uint8_t tmp = grid[y1][x1];
    grid[y1][x1] = grid[y2][x2];
    grid[y2][x2] = tmp;

    uint8_t to_clear[GRID_SIZE][GRID_SIZE];
    uint8_t matched = find_matches(to_clear);
    if (!matched) {
        tmp = grid[y1][x1];
        grid[y1][x1] = grid[y2][x2];
        grid[y2][x2] = tmp;
        return 0;
    }

    uint16_t total_cleared = 0;
    do {
        total_cleared += matched;
        collapse_and_refill(to_clear);
    } while ((matched = find_matches(to_clear)) != 0);

    board_redraw();
    return total_cleared;
}
