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

## Status: Milestone 10 (a bigger map)

Milestones 1-4 built a small bordered-room grid with a freely-walking,
collision-checked, combat-capable player: a one-shot sword swing, one
patrolling enemy confined to room (0,0), 3 hearts with a sprite-based
HUD, enemy contact damage with brief invincibility, and one heart
pickup that heals. Milestone 5 made one room a real, otherwise-
unreachable goal room, gated behind a locked door that a key unlocks.
Milestone 6 added a real win condition — defeat the enemy *and* reach
the goal room for a one-shot "WIN" screen. Milestone 7 added five
sound effects. Milestone 8 added a "WAYFARER" / "PRESS START" title
screen. Milestone 9 added real battery-backed SRAM save (key/pickup/
enemy-defeated/won state persists across power cycles) — completing
the roadmap's original stretch list.

User-directed next step: add more rooms to the map. Grew from the
original 2x2 grid to 3x2 — two new, empty rooms at (2,0) and (2,1).
**The entire code change was one constant**, `GRID_W` 2 → 3 in
`src/world.c`: `compute_sides()`'s per-side logic, `room.c`'s
per-cell wall/corner/door rendering, and `room_blocks()`'s collision
were all already general enough to grow the grid with no other
changes — confirmed by re-reading the code fresh, not assumed. The two
new rooms open naturally off the existing key room (1,0) and heart
pickup room (1,1) (previously dead-end walls on their east sides),
forming a real explorable loop alongside the existing linear critical
path to the win room, rather than a dead-end appendage — verified by
walking the full loop and confirming each room's own wall-opening
fingerprint. The critical path itself (the severed edge, the key lock,
the win condition) is untouched; the existing win-condition script's
own final frame, audio, and save file were all reconfirmed unaffected
via direct `cmp` before locking in — except the audio, which needed
re-locking in place again: even this one-constant change measurably
shifted exact sample-level SFX timing (not frame timing), the same
category of finding Milestone 9 already hit once.
