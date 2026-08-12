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

## Status: Milestone 17 (a treasure chest)

Milestones 1-4 built a small bordered-room grid with a freely-walking,
collision-checked, combat-capable player: a one-shot sword swing, one
patrolling enemy confined to room (0,0), 3 hearts with a sprite-based
HUD, enemy contact damage with brief invincibility, and one heart
pickup that heals. Milestone 5 made one room a real, otherwise-
unreachable goal room, gated behind a locked door that a key unlocks.
Milestone 6 added a real win condition — defeat the enemy *and* reach
the goal room for a one-shot "WIN" screen. Milestone 7 added five
sound effects. Milestone 8 added a "WAYFARER" / "PRESS START" title
screen. Milestone 9 added real battery-backed SRAM save. Milestone 10
grew the map to a 3x2 grid with an explorable loop. Milestone 11 added
a way to restart from the win screen (with a "PRESS START" hint added
just after). Milestone 12 populated one of Milestone 10's two empty
rooms with a second, optional enemy (and fixed a real sprite-tile-ID
bug found right after). Milestone 13 makes the sword itself a pickup —
the player starts unarmed. Milestone 14 adds a shield pickup with real
directional blocking, closing out the last of Milestone 10's two empty
rooms. Milestone 15 adds a looping background theme. Milestone 16 grows
the map to 3x3 and adds an optional boss, the real payoff for both
equipment pieces working together. Milestone 17 adds a treasure chest
that permanently raises max hearts from 3 to 4.

**Real bug report**: Milestone 9's SRAM save meant a `won` save loaded
straight back into the win screen (by design) — but there was never a
way *out*. Once you'd won and saved, the game was permanently stuck
showing "WIN" on every future boot, a real gap Milestone 9's own plan
had flagged but not closed.

`src/world.c`'s existing `world_init()` body was factored into a new
`reset_world()` — the exact same setup sequence a fresh boot already
used, now shared rather than duplicated by a new `restart_game()`
(new `sram_reset()` in `src/sram.c` wipes the save back to fresh
first, wrapped in the same real-hardware-safe screen-off pattern
`go_to_room()`/`win_play()` already use, since this is a bulk visual
reset happening mid-session). While the win screen is showing, a
`Start` press now triggers it — **scoped deliberately narrow to the
actual reported problem**: restart is only reachable from the win
screen, not a general "restart anytime" button during normal play
(unlike the sibling `prism/` project's own always-available restart),
and goes straight back into fresh gameplay at room (0,0) rather than
through the title screen again, matching `prism/`'s own
`restart_game()` precedent.

Verified end to end across two separate process invocations: playing
to a win and pressing `Start` resumes real gameplay (fresh hearts, the
enemy patrolling again, the key/pickup back in their rooms) and leaves
behind a genuinely fresh `.sav` (magic + all-zero, not `won`) — then a
*second*, independent process loading that fresh save confirms a
normal boot with no immediate win screen, the real proof the fix
works rather than just that in-memory state looked right. The existing
"a won save shows the win screen" behavior (Milestone 9) is untouched
and still separately verified — restart only adds an exit from it, not
a replacement for it.

**Follow-up**: the win screen originally said only "WIN", with nothing
telling the player `Start` does anything — the restart worked but was
undiscoverable. `src/win.c` now draws "PRESS START" beneath it, reusing
`title.c`'s own P/R/E/S/T/A letter tiles (same font, same bytes) and a
`draw_centered()` helper mirroring `title.c`'s own. Verified visually
(the win screen's reference frame was intentionally regenerated) and
re-locked the SFX reference after confirming — via `analyze_sfx.py` —
that the same events still occur at the same approximate timestamps,
just at a slightly shifted exact sample position (the same "real code
change shifts sample position, not frame timing" finding as the last
two milestones).

**Milestone 12**: room (2,1), one of the two rooms Milestone 10 left as
empty exploration space, now holds "the brute" — a bigger (16x16 vs.
the original enemy's 8x8), violet-colored enemy that patrols
vertically and takes two hits to kill. Deliberately **optional**: the
win condition still only requires defeating the original enemy and
reaching the goal room, so this is pure extra content, not a harder
required path.

`src/brute.c`/`brute.h` mirror `enemy.c`/`enemy.h`'s shape: a 2x2
metasprite (four 8x8 tiles instead of one), its own OBJ palette, and a
new post-hit cooldown (`BRUTE_HIT_COOLDOWN_FRAMES`, comfortably longer
than the sword's own 12-frame active window) — without it, a single
sword swing would register as several hits in a row, since `world.c`
re-checks the hit-test every frame the blade is out. `brute_try_hit()`
returns a 3-state result (no hit / hit-but-alive / hit-and-defeated) so
`world.c` can play a new, distinct sound (`sfx_play_brute_hit()`,
channel 2 — previously entirely silent, now routed via `sfx_init()`'s
`NR51_REG`) on the first hit, and reuse the existing `sfx_play_hit()`
on the second, lethal one. A new `BIT_BRUTE` SRAM flag persists
defeat, same shape as the original enemy's own flag.

Verified with a scripted playthrough that deliberately routes around
the *original* enemy's own patrol row (so this test's own contact-
damage exposure is only ever from the brute) before crossing into room
(2,1) and landing two real swings — the exact working frames found by
scanning against the actual build (temporarily instrumented with a
one-off serial print, removed once confirmed), not computed by hand.
The script also includes a deliberate extra swing *inside* the
cooldown window, confirmed to be a genuine no-op, proving the double-
hit guard actually works rather than assuming it does. As predicted,
touching `sfx_init()` shifted the existing main-script audio
reference's exact sample timing (the new channel routing runs before
any other sound) — confirmed via `analyze_sfx.py` that the same events
still land at the same approximate timestamps, then re-locked in
place, the same treatment the last three milestones all needed.

**Bug fix, found right after**: the brute's own 4 tile IDs (15-18)
collided with `heart_hud.c`'s tile 15 and `key.c`'s tile 16 - since
`reset_world()` initializes those two modules *after* the brute, their
own tile data silently overwrote 2 of the brute's 4 quadrants (a heart
and a key silhouette rendered in its own violet palette, in place of
its actual blob shape) whenever the brute was alive and visible.
Invisible in the existing post-defeat reference frame, so never
caught. Fixed by moving the brute to its true next-free tile range
(17-20); a new reference frame captured while the brute is still
alive closes the actual test-coverage gap.

**Milestone 13**: the player now starts every session unarmed — `A`
does nothing at all until the sword itself is found as a real pickup
in room (0,0), sitting on the same walking line to the original enemy
the player was already taking. `src/sword_pickup.c`/`.h` mirror
`key.c`'s shape closely, and deliberately claim **zero new sprite tile
or palette IDs** — they draw the exact same blade art `sword.c` already
owns (now exported as `SWORD_TILE_ID`/`SWORD_OBJ_PALETTE` in `sword.h`,
the single source of truth, rather than a second private copy of the
same constants) rather than repeat the exact class of mistake the
Milestone 12 follow-up above just fixed. `world.c` gates the entire
swing trigger behind `sword_pickup_is_collected()`; nothing in
`enemy.c`/`brute.c` needed to change at all, since `sword_is_active()`
already reads false forever until the first real swing happens.

Gating combat behind a pickup is a real, global behavior change —
every existing script that swings assumed the player already had a
sword. Placing the pickup exactly on `input_script_m8.txt`'s own
already-scripted walking path meant that script (and `input_script_m11.txt`,
which extends it as a literal prefix) needed **zero frame-number
changes** — confirmed by actually rebuilding and running them, not
assumed. `input_script_m12_brute.txt`'s own path deliberately avoids
that exact row for unrelated reasons, so it needed a real new detour;
its two combat hit frames were re-scanned from scratch against the
real build rather than assumed to shift by the same fixed offset (they
didn't, quite — a real, confirmed instance of this project's own
"don't trust a uniform shift for combat timing" rule). `reference_m8.sav`/
`reference_m11.sav`/`reference_m12_brute.sav` all gained a new
`BIT_SWORD` bit (or, for the brute's own reference, needed regenerating
along with its WAV for the new detour); `reference_m11_sfx.wav`/
`reference_m12_brute_sfx.wav` both needed re-locking, one more real
instance of "any code-size change can shift exact sample timing."

**Milestone 14**: a shield pickup in room (2,0), the last of the two
rooms Milestone 10 left empty. Blocks contact damage - but only
*directionally*, a real skill element (classic Zelda-style blocking):
`shield_blocks()` compares the box centers of the player and the
threat, picks the dominant axis (the larger of the x/y distance, ties
favoring horizontal), and requires the player's current facing to
point at that same side - not merely "shield equipped." `shield.c`
claims **zero new sprite tile/palette IDs** (a real, new hand-drawn
heater-shield tile, but reusing the sprite-ID-registry discipline the
Milestone 12 follow-up established) and needs just one new OAM slot.
Both existing contact-damage sites (the enemy and the brute) route
through this same check now; a new edge-triggered `sfx_play_block()`
(channel 2, a bright "ting" distinct from the brute's own low "thud")
plays once per block, mirroring the existing `was_swinging` idiom
since a block has no invincibility timer of its own to naturally gate
repeat frames.

Verified against the brute (closer to the new pickup than the
original enemy, so the round trip is far shorter) with a real,
un-scripted demonstration of the mechanic's own directional nature:
walking straight at it (facing matches its position) blocks contact
correctly, then - without any extra scripting - the brute's own
continuous patrol carries it out of the faced direction mid-contact,
and blocking correctly stops, taking exactly one real hit. Confirmed
directly via a temporary serial-instrumented probe before it was
removed, plus 10 hardcoded synthetic geometry cases covering all 4
directions and the horizontal-wins-tie edge case (SDCC's optimizer
flags this function with a well-known, unrelated false-positive
warning - verified benign against the real build, documented inline
rather than silenced). Two locked references mirror Milestone 12's own
"alive" + "defeated" two-checkpoint shape: still-blocked (2 hearts)
and post-transition (1 heart) - real regression coverage for both
states, not just the net result. `reference_m11_sfx.wav`/
`reference_m12_brute_sfx.wav` both needed re-locking again (same
"any code-size change can shift exact sample timing" finding, a fourth
time now); `input_script_m8.txt`/`m11.txt`/`m12_brute.txt` themselves
needed no changes at all, since the shield's own room placement was
chosen off every existing script's own walked path.

**Milestone 15**: a looping background theme on channel 3 (the wave
channel) — entirely unused until now, so music and every existing sfx
mix in hardware for free, no ducking logic needed. `src/music.c`/`.h`
programs a soft 32-sample triangle wave (deliberately a different
*timbre* from every sfx's own sharp pulse tone) and steps through a
short, original 16-note phrase (not a transcription of any real game's
theme) at ~120 BPM, looping every ~9s. Starts at `world_init()`/
`reset_world()` (so Milestone 11's own restart restarts the theme too)
and stops the moment the player wins, so it doesn't compete with the
win jingle. Channel 3 uses a genuinely different frequency formula
from channels 1/2 (`65536/(2048-period)`, not `131072/...` — confirmed
directly against the emulator's own `src/apu.c` timer reload, not
assumed the same) — `sfx.c`'s own top comment, which previously
overgeneralized this, was corrected in the same commit.

**A real verification-scope escalation**, called out honestly rather
than downplayed: every prior WAV-reference change in this project
shifted *when* existing sounds land or added one new one-shot event.
This one adds continuous new audio *throughout* every existing
script's entire runtime, from the moment gameplay begins — a
qualitatively bigger change than a re-lock. `analyze_sfx.py`'s
original near-silence-threshold event detector broke immediately
(everything merged into one giant "event" once music kept the RMS
elevated continuously) even after halving the music's own volume from
50% to 25%, since no volume level fixes "the background never actually
goes quiet." Fixed with a real upgrade — a second, peak-relative
detection mode comparing each window's RMS against a rolling local
baseline (median of a surrounding window) rather than a fixed near-
zero floor — which correctly separated every real sfx event back out
in all three existing scripts, confirmed at the same timestamps as
before, plus surfaced two genuine, honest, minor findings: a tiny
audible "click" transient at the exact moment Milestone 11's own
restart re-initializes the wave channel, and equally tiny blips at
each note's own retrigger transient — both real, both harmless,
neither hidden. The dedicated `input_script_m15_music.txt` (title
dismiss only, no other input) isolates the melody alone; correctness
was verified against the built ROM's own actual audio output, not
assumed from the source — sampling dominant frequency at 8 of the 16
steps' own midpoints (via a small DFT-based probe, not
`analyze_sfx.py`'s own event detector) confirmed each matches its
intended note within measurement resolution, the rest step goes
genuinely silent, and the loop restarts cleanly at the top.

**Milestone 16**: an optional boss, the payoff for the sword and
shield working together. The map grows to 3x3 - a new dead-end room
(2,2) south of the brute's own (2,1) - reached the same way Milestone
10's own grid growth was: `compute_sides()` already computes plain
grid topology by default, so growing `GRID_H` only needed two new
override lines to *prevent* unwanted connections, not build new ones.
`src/boss.c`/`.h` mirror `brute.c`'s shape: a 24x24 blob (bigger than
the brute's own 16x16), bouncing independently on *both* axes (a real
first - every earlier enemy here moves on one axis only), three hits
to die, and contact damage that routes through `shield_blocks()`
completely unchanged - the actual mechanism the equipment payoff runs
on, needing zero new blocking logic.

**A real bug found by actually looking, not just reasoning about the
code**: CGB hardware has only 8 OBJ palette slots, and every existing
module here already claimed one (player, sword, enemy, heart_hud x2,
key, brute, shield - all 8, confirmed against
`docs/HARDWARE_REFERENCE.md`). An early draft gave the boss a 9th
(index 8) anyway; CGB's OCPS palette index is only 6 bits wide, so
that write silently wrapped around and overwrote *palette 0* - the
player's own colors. Invisible from reading the source, glaringly
obvious the moment it was actually rendered: the player sprite turned
up in the boss's own crimson. Fixed by exporting `BRUTE_OBJ_PALETTE`
from `brute.h` (the single source of truth; `brute.c` itself now
references it too, not a private duplicate) and having the boss
deliberately reuse it - a real hardware constraint, not a shortcut.

Two locked references again mirror the brute's own "alive" +
"defeated" two-checkpoint shape. The three real hit frames were found
the same empirical way the brute's own were (a temporary serial-
instrumented scan against the real build, removed once confirmed) -
the boss's own independent two-axis bounce made this even less hand-
tractable than the brute's single-axis case ever was. Growing the map
broke two existing scripts in a way worth being honest about:
`input_script_m12_brute.txt`'s and `input_script_m14_shield.txt`'s own
DOWN-holds both used to rely on `(2,1)`'s south wall being closed to
safely overshoot and clamp - now that it's open (leading to the new
room), both scripts walked straight through into `(2,2)` instead,
confirmed via `cmp`, not assumed safe. Fixed with precise, non-
overshooting hold durations instead, and re-verified (not just
patched and hoped): `input_script_m12_brute.txt` needed its combat
timing re-derived from scratch (the room-entry frame itself didn't
move, but the approach got faster, so the old hit frames no longer
lined up), while `input_script_m14_shield.txt`'s own block/unblock
checkpoints turned out to still land at the exact same frame numbers,
confirmed rather than assumed, since the player's own trajectory up to
its new, earlier stopping point was unchanged. Every existing WAV
reference needed re-locking again (a fifth real instance of "any
code-size change can shift exact sample timing," now from the map
growth itself). Full regression suite stayed green throughout,
confirmed via a full `make clean` rebuild.

**Milestone 17**: a treasure chest in room (2,0) - the shield's own
room, the second precedent (after (0,0)'s enemy + sword_pickup) for two
independent pickups sharing a room. Grants a real, permanent
progression reward - max hearts rises from 3 to 4 (`player.c`'s new
`player_increase_max_hearts()`), not just a full heal - the first time
`player.c`/`heart_hud.c`'s core heart system has changed since
Milestones 1-2.

**Two registries were already fully saturated going in**: SRAM's single
state byte (all 8 bits claimed as of the boss's own `BIT_BOSS`) and all
8 CGB OBJ palette slots. A new `_SRAM[2]` second state byte holds the
new `BIT_CHEST` flag - the exact contingency `sram.h`'s own comment had
already named. The chest's own tile reuses `key.c`'s gold palette
(newly exported as `KEY_PALETTE`, the same "reuse an existing palette,
document why" move the boss already made with `BRUTE_OBJ_PALETTE`) -
the key and the chest never appear on screen at the same time, so
sharing the index is safe.

`heart_hud.c`'s own sprite slots 0-25 were fully claimed, so the 4th
heart couldn't extend the original contiguous `HEART_SPRITE_BASE + i`
scheme (slot 9 already belonged to `pickup.c`) - it gets slot 26
instead, via a new explicit `heart_slots[4] = {6, 7, 8, 26}` array. Both
`heart_hud_init()`/`heart_hud_update()` now loop over the player's real
current max instead of a fixed 3, hiding any slot not yet unlocked -
the same "safe to call unconditionally" contract this function already
had. **A real correctness fix alongside it**: `player_heal_full()` used
to hard-reset to the old fixed `MAX_HEARTS` constant, which would have
silently un-done the chest's own reward on the very next heart pickup
or death-respawn - now reads the real, current `max_hearts` variable.

Verified with a fully unarmed script (no sword needed to collect a
chest) and two checkpoints: chest collected (HUD now shows 4 full
hearts - real collection confirmed, via a direct bisection scan against
the built ROM, to happen as soon as the player's box overlaps the
chest's, well before reaching its exact center) and, the real point of
this milestone's own test coverage, one deliberate unarmed graze taken
afterward from the brute - proof `heart_hud.c`'s generalization renders
partial damage correctly at the new 4-heart width (3 full + 1 empty),
not just "still shows 3 hearts total." Every prior PPM/`.sav` reference
came back byte-identical via `cmp` (unaffected by this change); every
prior WAV reference needed the now-familiar re-lock-and-verify pass (a
sixth instance of "any code-size change can shift exact sample timing,"
confirmed via `analyze_sfx2.py` before re-locking). Full regression
suite stayed green throughout, confirmed via a full `make clean`
rebuild.
