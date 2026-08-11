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

## Status: Milestone 8 (title screen)

Milestones 1-4 built a small 2x2 grid of bordered rooms with a
freely-walking, collision-checked, combat-capable player: a one-shot
sword swing, one patrolling enemy confined to room (0,0), 3 hearts with
a sprite-based HUD, enemy contact damage with brief invincibility, and
one heart pickup in room (1,1) that heals. Milestone 5 made room (0,1)
a real, otherwise-unreachable goal room, gated behind a locked door
that a key (in room (1,0)) unlocks. Milestone 6 added a real win
condition — defeat the enemy *and* reach room (0,1) for a one-shot
"WIN" screen. Milestone 7 added five sound effects (swing/hit/pickup/
damage/win), direct DMG/CGB register pokes.

Of the two remaining stretch items, the user picked the title screen.
New `src/title.c`/`title.h` is the third application of the same
hand-authored 5x7 block-letter, real-hardware-safe-LCD-timing technique
this project has now used three times — the sibling `prism/` project's
own `title.c` first, this project's own `win.c` (Milestone 6) second,
this one third. `title_screen()` shows "WAYFARER" / "PRESS START",
blocking until `Start` is pressed, before `world_init()` ever runs.

**A real, unavoidable consequence, not an oversight**: the title screen
is a genuine player-paced wait, shifting every previously-scripted
frame number — the same thing `prism/`'s own Milestone 6b hit. Every
frame in the verification script needed re-deriving from scratch, and
a real, worth-stating finding came out of it: the movement-duration
legs (holding a direction for N frames) turned out to be
shift-invariant (an N-frame hold always moves N pixels, regardless of
when gameplay actually started), but the sword-kill's own precise
timing was *not* — scanning candidate frames found the real working hit
5 frames earlier than a naive uniform-shift guess would have predicted.
A uniform-shift assumption would have produced a same-looking-but-wrong
script (the enemy would have still been alive at the scripted swing) —
exactly the kind of thing this project's own "derive empirically, don't
assume timing transfers" discipline exists to catch.

Verified via a new scripted `--input` sequence (`input_script_m8.txt`,
replacing `input_script_m6.txt`) covering the full path again from a
fresh title-screen boot: dismiss the title, defeat the enemy, collect
the key, cross to the goal room, and confirm the win screen — locked in
as `reference_m8.ppm`/`reference_m8_sfx.wav` (replacing
`reference_m6.ppm`/`reference_m7_sfx.wav`, deleted once the new ones
were confirmed).
