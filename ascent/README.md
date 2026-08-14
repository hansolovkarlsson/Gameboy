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

## Status: Milestone 4 (a goal and a win screen)

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
tile at the player's feet is a ladder tile and Up/Down is held — this
naturally self-limits, since stepping past the ladder tile into open
air or a plain floor tile means the very next frame's check no longer
grants a climb).

Milestone 1 deliberately shipped no jump, no barrels, no goal, no HUD,
no sound — the platformer-genre equivalent of `wayfarer/`'s own real
Milestone 1 (a player sprite, wall collision, nothing else).

Milestone 2 adds the pairing that Milestone 1's own "Next" note already
flagged - a jump is barrel-dodging tech, so the two are scoped
together, not bolted on separately. A barrel hitting the player
respawns them at the ground (confirmed by the user over a fuller
lives/score/game-over system, which stays an honest later milestone -
the same "core mechanic first, consequence system layered on later"
shape `wayfarer/` itself followed with enemies-then-hearts).

`src/player.c`'s jump is a timed, fixed 20px-rise-then-fall arc (with
Left/Right air control the whole flight) - the one genuinely stateful
addition on top of Milestone 1's otherwise-stateless per-frame tile
checks, since a jump is a discrete timed action, not something
rederivable fresh from the map every frame the way gravity/ladder-grip
already are.

New `src/barrel.c` spawns up to two 8x8 barrels on the top platform,
periodically. A barrel rolls 1px/frame until it's resting over a
ladder-top tile, then descends 1px/frame through the ladder shaft
until it lands on the tier below, reversing direction each time - two
simple rules that, against this stage's real tile map, are enough to
make barrels retrace the player's own climb route in reverse with no
per-tier special-casing (column 4 carries the ground↔tier1 and
tier2↔tier3 legs; column 14 carries tier1↔tier2 - a barrel spawned
rolling toward column 4 on tier 3 ends up sweeping the *entire* zigzag
before rolling off the ground edge and despawning).

**A real tuning finding, not just a script-timing one**: the jump's
first height (12px, sized against a barrel's own static 8px footprint)
could not actually survive a real *moving* barrel - an approaching
barrel and the player overlap horizontally for ~22-24 frames as it
closes in, but a 12px hop is only high enough to clear it for about 9
frames around its peak, confirmed by testing several jump-start frames
against a real rolling barrel and getting caught every time. Widening
the rise to 20px (a ~21-25 frame clearance window) was what actually
made a full pass survivable.

**Verification**: `input_script_m2_barrels.txt` reuses Milestone 1's
own route verbatim (still byte-identical against `reference_m1.ppm`,
since `SPAWN_INTERVAL` is deliberately tuned so the first barrel
doesn't spawn until after a real climb could reasonably finish), walks
to open ground on tier 3, then jumps a real oncoming barrel
(**checkpoint A**: still resting in place, not reset) before
deliberately taking a hit from the next one (**checkpoint B**: back at
the ground spawn point). Every frame number - the walk, the jump
window, the hit - was found by bisecting against the real built ROM,
not hand-derived, the same discipline Milestone 1's own ladder-
alignment windows already used.

Milestone 3 fixes a real gap the user found right after trying Milestone
2: the player could climb *up* a ladder from a resting position but not
*down* one. The ladder-grip check had been keyed on the player's
vertical center (`player_y + 8`), which happens to work for climbing up
- a resting player's own box already overlaps the shaft leading up from
wherever they're standing - but a shaft leading *down* starts on the
far side of the solid tile underfoot, which the center point never
reaches. Rekeying the check to the player's *feet* (`player_y + 16`,
the exact point the solid/gravity check already uses) makes grip
detection symmetric in both directions, since it's now checking "is the
tile I'm standing on itself a ladder-top junction" rather than an
offset that only happened to line up one way. A new explicit clamp
(`player_y < GROUND_REST_Y`) stops the player exactly at true ground
level while descending - without it, `stage_tile_at()`'s own clamping
of any row past the map's last row means the ground's ladder-top tile
would keep reading as climbable forever, letting the player sink out of
the map while holding Down. Confirmed this change doesn't alter a
single existing frame of Milestone 1's or Milestone 2's own locked
references (re-run and `cmp`'d byte-identical) before adding a
dedicated verification script for the new behavior itself.

`input_script_m3_climbdown.txt` reuses Milestone 1's own ground→tier1
climb (frames 5-114), then holds Down and confirms (**checkpoint**,
frame 250) the player is back at true ground rest directly below where
they started - proof descending actually completes, not just grips
without moving.

Milestone 4 adds a real win condition. New `src/goal.c` is a single
stationary flag, fixed on tier 3 past column 4 - deliberately placed
where no barrel ever reaches, since `barrel.c`'s own barrels always
descend at the first ladder column they cross and column 4 is tier 3's
only one, confirmed against the real tile map rather than assumed.
Reaching it (a plain AABB check against the player's box, the same
shape `barrel_check_hit()` already uses) is a one-way trigger: `main.c`
tracks a single `won` flag that, once set, stops calling
`player_update()`/`barrel_update()` entirely and calls new
`src/win.c`'s `win_play()` - a one-shot screen wipe to a plain "WIN"
message, reusing `wayfarer/src/win.c`'s own hand-authored W/I/N letter
tiles verbatim (same reasoning as reusing its player sprite art
elsewhere in this project). No restart yet, and no lives/score/sound
either - the platformer-genre equivalent of `wayfarer/`'s own real
Milestone 6, which shipped a terminal win screen alone, restart arriving
only much later at Milestone 11.

**A real, expected ripple effect, not a bug**: this is one static,
never-scrolling screen, so the goal flag - drawn once at startup and
never hidden until a win - is visible in *every* frame from now on,
including every earlier milestone's own locked references. Confirmed
directly (re-running each existing script showed an expected `cmp`
mismatch, always at the same byte offset, always just the flag's own
pixels) before re-locking `reference_m1.ppm`,
`reference_m2_survive.ppm`, `reference_m2_respawn.ppm`, and
`reference_m3_climbdown.ppm` alongside this milestone's own new
reference - not silently overwritten, verified first.

**Verification**: new `input_script_m4_win.txt` reuses Milestone 1's
own full climb (frames 5-374) verbatim, then walks further left to the
flag - no barrel risk on this leg either, for the same reason the flag
itself is safe there. The actual contact frame was bisected against the
real build to between frame 386 and 387. One PPM reference
(`reference_m4_win.ppm`, frame 450) checkpoints the screen showing only
the "WIN" text - the stage, the player, both barrel sprites, and the
goal flag itself are all gone at once, confirmed stable (not reverting)
by rendering a later frame and finding it identical.
