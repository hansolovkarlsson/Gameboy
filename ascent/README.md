# Ascent (working title)

This project's third original homebrew game, and a third distinct
genre from its two siblings ([`prism/`](../prism/README.md), a
grid-based match-3 puzzle, and [`wayfarer/`](../wayfarer/README.md), a
top-down action-adventure): a CGB-only, single-screen platformer in
the tradition of the original Donkey Kong (1981) — stacked girders and
zigzagging ladders, climbed from bottom to top. See
`docs/GAMEBOY_ROADMAP.md` for the full concept writeup and milestone
roadmap; this file just covers the toolchain and where things live.

## Toolchain: GBDK-2020 (shared with `prism/`/`wayfarer/`)

Written in C using [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020),
same as `prism/`/`wayfarer/` — see `prism/README.md`'s own toolchain
section for the install steps and GPLv2+LE licensing note (no need to
repeat a third copy here).

This project's own `Makefile` deliberately does **not** vendor a third
copy of the toolchain — `GBDK_HOME` defaults to
`../prism/toolchain/gbdk`, reusing the exact same install `prism/` and
`wayfarer/` already need. If you've already built either sibling,
nothing extra to install; just run `make gameboy-ascent-build` from
the repo root. Override `GBDK_HOME` if you'd rather point at an
independent install.

## Building

```
make gameboy-ascent-build   # from the repo root - opt-in, needs GBDK-2020 installed
```
Builds `ascent/bin/ascent.gb` (via `make -C ascent`) and smoke-runs it
through this project's own `bin/gameboy --mode cgb`, same treatment
`gameboy-prism-build`/`gameboy-wayfarer-build`/`gameboy-sdl`/the RGBDS
targets get: opt-in, never part of plain `make`/`make gameboy-test`.

## Status: Milestone 1 (gravity, platforms, ladders)

One static 20x18-tile screen (`src/stage.c`): four girder tiers
(rows 5, 9, 13, 17 of a 18-row screen) connected by two ladder columns
(x=4 and x=14) that zigzag — column 4 connects the ground to tier 1
and tier 2 to tier 3 (the top); column 14 connects tier 1 to tier 2 —
built from four simple 8x8 tiles (open air, girder, ladder, and a
girder-with-ladder-passing-through "ladder-top" junction tile) rather
than sloped platforms, which pixel-tile collision can't cleanly
express.

`src/player.c` gives the player (a 16x16 side-profile sprite reusing
`wayfarer/src/player.c`'s own proven 4-quadrant, mirror-for-facing
technique and tile art) three things, checked fresh every frame
against the tile map with no persistent state machine: gravity (fall
1px/frame when the tile below the feet isn't solid), platform standing
(snap to rest exactly on a girder's top edge, walk left/right while
grounded), and ladder climbing (move 1px/frame vertically while the
player's center overlaps a ladder tile and Up/Down is held — this
naturally self-limits, since stepping past the ladder tile into open
air or a plain floor tile means the very next frame's check no longer
grants a climb).

No jump, no barrels, no goal, no HUD, no sound yet — deliberately
minimal, the platformer-genre equivalent of `wayfarer/`'s own real
Milestone 1 (a player sprite, wall collision, nothing else). Jumping
in particular is barrel-dodging tech, so it's honestly scoped
alongside barrels in a later milestone rather than added here just
because Donkey Kong "has" a jump button.

**Verification**: `input_script_m1.txt` walks a single continuous
route across the whole screen — ground → tier 1 (via the x=4 ladder) →
tier 2 (via the x=14 ladder, proving the second column independently)
→ tier 3, the top (via the x=4 ladder again) — with the player
starting 8px above true rest height, so the opening frames' fall onto
the ground girder is itself a free proof that gravity/landing works.
Every leg's exact frame count (how long to walk into a ladder's ~8px
horizontal grip window, how long to hold a climb) was found by
bisecting against the real built ROM, not hand-derived — the same
"verify against the real build" discipline this whole project already
holds itself to elsewhere. One PPM reference
(`reference_m1.ppm`) checkpoints the player at rest on the top
platform, confirmed stable (not still settling) by rendering a later
frame and finding it identical.
