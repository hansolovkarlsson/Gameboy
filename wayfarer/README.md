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

## Status: Milestone 3 (combat)

Milestone 1 built one static, fully-bordered room and a 16x16
directional player sprite that walks it freely (pixel-level, not
grid-snapped), stopped cleanly by wall collision. Milestone 2
(`src/world.c`) wired a small 2x2 grid of rooms together — stepping off
an open side's true screen edge cuts instantly to the adjacent room,
entering from its opposite edge, while a closed side still blocks
movement.

This pass adds combat: `src/sword.c` is a one-shot sword swing,
edge-triggered by `A` (movement isn't locked during a swing), an 8x8
blade sprite held one tile-width beyond whichever edge of the player
the current facing points at. `src/enemy.c` is one small patrolling
enemy (a round blob, deliberately simpler/smaller than the player),
confined to room (0,0), moving back and forth along a fixed range. A
sword hit defeats the enemy permanently for the session — **the enemy
cannot hurt the player yet**; player health/damage/game-over is real,
separate scope, deliberately deferred to its own later milestone
rather than folded in here, the same "one provable slice" discipline
every prior milestone used.

Verified via a scripted `--input` sequence (`input_script_m3.txt`)
that includes a real negative-case check first — an early swing nowhere
near the enemy provably does nothing to it — before approaching and
landing a real hit, confirmed by the enemy disappearing and staying
gone many frames later. Locked in as `reference_m3.ppm` (supersedes
`reference_m2.ppm`, deleted once the new one was confirmed).
