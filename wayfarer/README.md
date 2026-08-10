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

## Status: Milestone 6 (win condition)

Milestones 1-4 built a small 2x2 grid of bordered rooms with a
freely-walking, collision-checked, combat-capable player: a one-shot
sword swing, one patrolling enemy confined to room (0,0), 3 hearts with
a sprite-based HUD, enemy contact damage with brief invincibility, and
one heart pickup in room (1,1) that heals. Milestone 5 made room (0,1)
a real, otherwise-unreachable goal room, gated behind a locked door
that a key (in room (1,0)) unlocks.

The remaining roadmap item bundles four independent stretch features;
asked the user which to build first — **win condition**: defeat the
enemy *and* reach room (0,1) to end the session with a real win state.
Recommended since it needed no new infrastructure category (no APU
work, no SRAM) and now has a genuine payoff to display.

New `src/win.c`/`win.h`: a small, self-contained text overlay reusing
the sibling `prism/` project's own `title.c` technique (a hand-drawn
5x7 block-letter tileset), but only the 3 letters "WIN" actually needs.
`win_play()` uses this project's own real-hardware-safe screen-off
pattern, `HIDE_SPRITES` (one hardware macro that turns off the whole
OBJ layer at once, rather than `win.c` needing to know about and call
into every other module's own sprite state), blanks the whole screen
by reusing `room.c`'s already-loaded `FLOOR_TILE_ID` under a new gold
palette (the same "one tile, palette swap" trick `heart_hud.c`'s
hearts already use — no second blank-tile bitmap needed), and draws
"WIN" centered. A one-shot, terminal screen — `src/world.c` freezes
the whole game loop (`if (won) return;`) the instant it's shown.

Verified via a scripted `--input` sequence (`input_script_m6.txt`)
that defeats the enemy (Milestone 3's own sword-kill timing) and then
walks the Milestone 5 path to room (0,1) — confirming the win screen
appears and stays frozen many frames later. A real negative-case check
too: running the *prior* Milestone 5 script unchanged (reaches room
(0,1) but never attacks the enemy) against this milestone's build
shows no win screen at all — both conditions are genuinely required,
not just the room transition alone. Locked in as `reference_m6.ppm`
(supersedes `reference_m5.ppm`, deleted once the new one was
confirmed).
