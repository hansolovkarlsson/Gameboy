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

## Status: Milestone 1 (single-room movement)

A 16x16 directional player sprite walks freely (pixel-level, not
grid-snapped) around one static, fully-bordered room, stopped cleanly
by the wall ring on every side — collision checked independently per
axis, so the player can slide along a wall rather than sticking. Three
directional art sets (down/up/one side profile, the side profile
mirrored via hardware sprite flip for left vs right — see
`src/player.c`) swap automatically as the D-pad is held. No room
transitions, combat, items, or HUD yet — all explicitly deferred to
later milestones (see `docs/GAMEBOY_ROADMAP.md`), not silently
missing.

Verified via a scripted `--input` sequence
(`input_script_m1.txt`) that walks the player into all four walls in
turn — down, right, up, then left — confirming collision genuinely
stops movement at each wall (not just "didn't crash"), and that all
four directional art sets render correctly, before locking in
`reference_m1.ppm` as the committed regression reference.
