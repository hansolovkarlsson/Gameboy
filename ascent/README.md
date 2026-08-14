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

## Status: Milestone 7 (sound effects)

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

Milestone 5 gives the win screen its one way out: a Start press
restarts the whole game, the same escape `wayfarer/src/world.c`'s own
win screen already gives (Start-restart, edge-detected against a new
`prev_joy` in `main.c` so a held Start button fires the restart exactly
once, not every frame). `win.c` gained a "PRESS START" hint below the
"WIN" text, reusing the exact same hand-authored P/R/E/S/T/A letter
bytes `wayfarer/src/win.c` already draws for its own identical hint.
Restarting just re-runs the same `stage_init()`/`player_init()`/
`barrel_init()`/`goal_init()` sequence real boot already uses - each
one was confirmed to be a full, idempotent reset of its own module's
state (position, sprites, the barrel array, the spawn timer), so
nothing from the finished run carries forward, the same "no separate
reset path" discipline `wayfarer/src/world.c`'s own `restart_game()`
already follows (there, layered on top of an SRAM wipe this project
has no equivalent of, since Ascent has no save state at all).

**A real bug, caught by actually testing a restart rather than trusting
the code**: the very first rendered frame after a restart showed the
right stage *shapes* but through the win screen's own leftover gold
palette instead of the stage's proper navy/rust one. `stage_init()`
had only ever written BG tile *indices* (`set_bkg_tiles()`) - on a
fresh boot that's enough, since the CGB's separate per-tile palette
*attribute* map already defaults to 0. But `win_play()` explicitly
stamps that same whole-screen attribute map to its own `WIN_PALETTE`
for the "WIN"/"PRESS START" text, and nothing had ever stamped it back.
`stage_init()` now does that stamp itself (an explicit whole-screen
`set_bkg_attributes()` call, mirroring `win_play()`'s own), making it a
genuinely idempotent reset rather than one that only happened to work
the first time.

**A second, expected ripple effect**: adding the win screen's new
"PRESS START" line meant `reference_m4_win.ppm` needed re-locking too
(one more line of text on an otherwise-identical frame). Separately,
`stage_init()`'s own attribute-stamp fix does slightly more work before
the very first `vsync()` of a fresh boot, nudging exactly when gameplay
begins by a handful of CPU cycles - late enough to shift a few
movement-precise checkpoints (`reference_m1.ppm`,
`reference_m2_survive.ppm`, `reference_m2_respawn.ppm`,
`reference_m3_climbdown.ppm`, all of which end mid-route on an exact
bisected frame) by a pixel or two, though not the win screen's own
static Milestone 4 checkpoint, which is captured long after the player
has already stopped moving. Confirmed harmless by rendering each
affected frame and finding the game state visually correct before
re-locking every one of them.

**Verification**: new `input_script_m5_restart.txt` reuses Milestone
4's own full win route (frames 5-420) verbatim, waits past the already-
confirmed-stable win screen, then presses Start at frame 500. A
checkpoint at frame 550 confirms the stage, player (back at the ground
spawn point), and goal flag are all back exactly as a fresh boot draws
them, with no barrels yet (a fresh spawn timer). A further short walk
(frames 555-565) and a second checkpoint at frame 600 confirm the
restarted game is actually playable, not just visually reset - the
player responds to input and settles at a new position, not just
sitting frozen post-restart.

Milestone 6 adds a score. New `src/score.c`/`.h` draws a live 5-digit
counter on background row 0 - open air in `stage.c`'s own tile map
above tier 3, never touched by any girder or ladder tile, so nothing to
dodge. The digit glyphs are reused verbatim from `prism/src/hud.c`'s
own `hud_digit_tiles` (the same "already-proven hand-authored art,
don't redraw it" reasoning `player.c`/`win.c` already give for reusing
`wayfarer/`'s own art), loaded at BG tile IDs 13-22 (`stage.c` owns
0-3, `win.c` owns 4-12) with a new BG palette (`stage.c` owns 0,
`win.c` owns 1) - color 0 the same dark navy as the stage's own
backdrop, so the digits' own background blends in rather than reading
as a separate box.

Scoring itself is the same real Donkey Kong (1981) mechanic and point
value: 100 points for each barrel jumped over. `barrel.c` gained a
per-barrel `jumped` flag (cleared only when that slot spawns a fresh
barrel) and `barrel_check_jump_score()` - a horizontal-only overlap
test, gated to barrels within 24px of the player's own vertical
position (tiers are 32px apart and the jump's own 20px arc never
reaches a neighboring one, so this margin only ever matches a barrel
genuinely on the player's own tier) so an unrelated barrel elsewhere on
the stage can never pay out. `player.c` gained a small
`player_is_jumping()` accessor so `main.c` only calls this check while
a jump is actually in progress - walking into a barrel already trips
`barrel_check_hit()`'s own full AABB into a respawn instead, so the two
conditions can never fire on the same contact.

**A real, expected ripple effect, not a bug**: the score field is
non-blank ("00000") from the very first frame, so it's visible in every
capture from now on, the same "one static, never-scrolling screen"
consequence Milestone 4's own goal flag already established.
`reference_m1.ppm`, `reference_m2_survive.ppm`,
`reference_m2_respawn.ppm`, `reference_m3_climbdown.ppm`, and
`reference_m5_restart.ppm` were all re-rendered, visually confirmed
still correct (including a real "00100" readout on the two references
that capture a state after the scripted jump), and re-locked.
`reference_m4_win.ppm` needed no change - `win_play()` wipes row 0 too,
and its own script never jumps a barrel.

**Verification**: new `input_script_m6_score.txt` reuses Milestone 2's
own exact route and bisected jump timing (frames 5-477) - Milestone 2
already proved the jump survives contact; this script's own new claim
is what the score shows afterward. **Checkpoint** (frame 600, same
timing as Milestone 2's own checkpoint A): the score field reads
"00100" - one barrel's worth of points, paid out exactly once despite
the jump's own multi-frame overlap window with the barrel.

Milestone 7 adds sound effects. New `src/sfx.c`/`.h` follows the exact
same approach `prism/src/sfx.c` and `wayfarer/src/sfx.c` already
established - direct DMG/CGB sound-register pokes, one write-and-
restart per event, real hardware envelope/sweep/length-counter decay
doing the rest, no per-frame service loop. Three of the four tones
reuse those projects' own exact register values verbatim rather than
re-deriving them (same physical channel-1/2/4 hardware, same
frequency-to-period formula - nothing to recompute): `sfx_play_jump()`
is channel 1, identical to `wayfarer/src/sfx.c`'s own
`sfx_play_swing()`; `sfx_play_score()` is `sfx_play_pickup()`'s own
exact note, moved to channel 2 so a barrel cleared mid-flight never
steps on a jump blip still decaying; `sfx_play_hit()` is channel 4
(noise), identical to `sfx_play_damage()`; `sfx_play_win()` is channel
1 again (never concurrent with a jump - `main.c`'s own `won` flag stops
`player_update()` entirely once it fires), identical to
`wayfarer/src/sfx.c`'s own `sfx_play_win()`.

`main.c` wires all four into the existing per-frame checks rather than
teaching any other module about audio - the same "return state, let
main.c decide" shape `barrel_check_hit()`/`goal_check_reached()`
already use. A jump's own *start* needed a new edge-detected
`was_jumping` local in `main.c` (comparing consecutive
`player_is_jumping()` reads, the same edge-detection shape already used
for the Start-press restart) since `player_is_jumping()` alone stays
true for the jump's whole multi-frame flight.

**Verification**: WAV capture, not a screenshot - two references reuse
existing scripts rather than adding new ones, since both already
exercise every relevant event. `reference_m7_sfx.wav`
(`input_script_m2_barrels.txt`, 17s) covers jump, score, and hit in one
route; `reference_m7_win_sfx.wav` (`input_script_m4_win.txt`, 8s)
covers the win fanfare. Confirmed against the real build with a custom
RMS/zero-crossing timeline analyzer (the same discipline every prior
milestone's own bisection already used, applied to audio instead of
pixels) before locking either: the jump/score/hit reference shows three
distinct events at ~7.94s, ~8.08s, and ~15.62s - matching the
already-bisected jump-press frame (475), the barrel's own overlap
window immediately after, and the already-bisected hit-contact frame
(932-935) respectively, once converted to seconds at ~59.7 fps; the win
reference shows one ~200ms event at ~6.46s, matching the already-
bisected win-contact frame (386-387) and `sfx_play_win()`'s own
LENGTH(12) note duration.
