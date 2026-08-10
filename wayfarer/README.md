# Wayfarer (working title)

This project's second original homebrew game, and a deliberately
different genre from the sibling [`prism/`](../prism/README.md) (a
grid-based match-3 puzzle): a CGB-only, top-down action-adventure in
the tradition of the original Zelda (1986) — free pixel-level movement
around bordered rooms, viewed from directly above. See
`docs/GAMEBOY_ROADMAP.md` for the full concept writeup and milestone
roadmap; this file just covers the toolchain and where things live.

## Toolchain: GBDK-2020 (shared with `prism/`)

Written in C using [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020),
same as `prism/` — see `prism/README.md`'s own toolchain section for
the install steps and GPLv2+LE licensing note (no need to repeat both
copies here).

This project's own `Makefile` deliberately does **not** vendor a second
copy of the toolchain — `GBDK_HOME` defaults to
`../prism/toolchain/gbdk`, reusing the exact same install `prism/`
already needs. If you've only ever built `prism/`, nothing extra to
install; just run `make gameboy-wayfarer-build` from the repo root.
Override `GBDK_HOME` if you'd rather point at an independent install.

## Building

```
make gameboy-wayfarer-build   # from the repo root - opt-in, needs GBDK-2020 installed
```
Builds `wayfarer/bin/wayfarer.gb` (via `make -C wayfarer`) and
smoke-runs it through this project's own `bin/gameboy --mode cgb`,
same treatment `gameboy-prism-build`/`gameboy-sdl`/the RGBDS targets
get: opt-in, never part of plain `make`/`make gameboy-test`.

## Status: Milestone 2 (room transitions)

Milestone 1 built one static, fully-bordered room and a 16x16
directional player sprite that walks it freely (pixel-level, not
grid-snapped), stopped cleanly by wall collision checked independently
per axis (so the player can slide along a wall rather than sticking).
Three directional art sets (down/up/one side profile, the side profile
mirrored via hardware sprite flip for left vs right — see
`src/player.c`) swap automatically as the D-pad is held.

This pass (`src/world.c`) wires a small 2x2 grid of rooms together
(`src/room.c`'s `room_draw()` now takes which of a room's 4 sides have
a neighbor, walling off only the sides that don't). Stepping off an
*open* side's true screen edge cuts instantly to the adjacent room,
entering from its opposite edge — a hard cut, not a scroll, by design.
A closed side still blocks movement exactly like Milestone 1, now
proven in a non-origin room too. No combat, items, or HUD yet — all
explicitly deferred to later milestones (see
`docs/GAMEBOY_ROADMAP.md`), not silently missing.

Verified via a scripted `--input` sequence (`input_script_m2.txt`)
that crosses two chained transitions (east, then south) and confirms a
closed north side still blocks movement in between — each room's
distinct wall-opening pattern (every room in a 2x2 grid is a corner,
so each has exactly 2 open sides) doubles as an easy visual fingerprint
that the right room is on screen at each step. Locked in as
`reference_m2.ppm` (supersedes `reference_m1.ppm`, deleted once the new
one was confirmed).
