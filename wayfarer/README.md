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

## Status: Milestone 13 (the player starts without a sword)

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
the player starts unarmed.

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
