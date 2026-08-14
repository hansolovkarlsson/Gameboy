// One static, fixed 20x18-tile Donkey Kong (1981)-style screen: four
// girder tiers (rows 5, 9, 13, 17) connected by a zigzagging pair of
// ladder columns (x=4, x=14). See stage.c for the tile art and the
// full tile map. No room-to-room transitions - this is the whole
// screen, drawn once.

#ifndef ASCENT_STAGE_H
#define ASCENT_STAGE_H

#include <stdint.h>

#define STAGE_TILE_W 20
#define STAGE_TILE_H 18

// Tile IDs - exported so player.c's collision checks can compare
// stage_tile_at()'s return value directly.
#define EMPTY_TILE 0
#define FLOOR_TILE 1
#define LADDER_TILE 2
#define LADDER_TOP_TILE 3

void stage_init(void);

// Pixel-coordinate tile lookup, clamped to the map bounds. Returns one
// of the *_TILE ids above.
uint8_t stage_tile_at(uint8_t px, uint8_t py);

// True for FLOOR_TILE/LADDER_TOP_TILE - solid enough to stand on.
uint8_t stage_is_solid(uint8_t px, uint8_t py);

// True for LADDER_TILE/LADDER_TOP_TILE - grippable.
uint8_t stage_is_ladder(uint8_t px, uint8_t py);

#endif
