// Shared grid-layout constants - used by main.c (drawing the gem grid,
// Milestone 2) and cursor.c (positioning the cursor sprite over a grid
// cell, Milestone 3), so they live in their own small header rather
// than being duplicated or owned by either file specifically.

#ifndef PRISM_GRID_H
#define PRISM_GRID_H

#define GRID_SIZE 6
#define GRID_TILE_W (GRID_SIZE * 2)
#define GRID_TILE_H (GRID_SIZE * 2)
#define GRID_ORIGIN_X 4
#define GRID_ORIGIN_Y 3

#endif
