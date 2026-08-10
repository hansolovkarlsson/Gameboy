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

## Status: Milestone 7 (sound effects)

Milestones 1-4 built a small 2x2 grid of bordered rooms with a
freely-walking, collision-checked, combat-capable player: a one-shot
sword swing, one patrolling enemy confined to room (0,0), 3 hearts with
a sprite-based HUD, enemy contact damage with brief invincibility, and
one heart pickup in room (1,1) that heals. Milestone 5 made room (0,1)
a real, otherwise-unreachable goal room, gated behind a locked door
that a key (in room (1,0)) unlocks. Milestone 6 added a real win
condition — defeat the enemy *and* reach room (0,1) for a one-shot
"WIN" screen.

Of the two remaining stretch items (sound effects, a title screen, SRAM
save), the user picked sound effects. New `src/sfx.c`/`sfx.h` directly
reuses the sibling `prism/` project's own `sfx.c` approach — raw DMG/
CGB sound-register pokes (this emulator's own APU already fully
verified), no higher-level GBDK sound API, no tracker: five one-shot
triggers, each a single write-and-restart to one channel's registers.
`sfx_play_swing()`/`sfx_play_hit()` mirror `prism/`'s own
`sfx_play_select()`/`sfx_play_match()` almost exactly (same notes,
same shapes); `sfx_play_pickup()` is a distinct-pitched blip shared by
both the heart pickup and the key (a real, deliberate scope decision —
both are "you got something," not worth telling apart audibly);
`sfx_play_damage()` uses channel 4 (noise), shorter and higher-pitched
than `prism/`'s own `sfx_play_gameover()` since it's a routine hit, not
a session-ending event; `sfx_play_win()` is the same rising-sweep shape
as the hit sound, just held longer.

Every `sfx_play_*()` call lives in `src/world.c` (the orchestrator),
never inside `sword.c`/`enemy.c`/`pickup.c`/`key.c`/`win.c` themselves
— those modules stay unaware audio exists at all, matching `prism/`'s
own `board.c`/`swapanim.c` layering relative to its `sfx.c`.
`player_damage()`'s return type changed from `void` to `uint8_t` (1 if
damage was actually applied, 0 if it was a no-op due to invincibility)
so the damage sound only plays on a real hit.

Verified the same way `prism/`'s own sound-effects milestone was:
`input_script_m6.txt` reused **unchanged** (confirmed byte-identical
rendered frame — sound is audio-only) with a new `--wav` capture,
checked programmatically rather than eyeballed. All 4 real trigger
points this script reaches show up as clean, distinct, correctly-timed
audio events with genuine silence between them. The heart-pickup call
site specifically (not reachable by this script) was verified once via
a separate, uncommitted probe — the same asymmetric-verification
precedent `prism/`'s own game-over sound used. Locked in as
`reference_m7_sfx.wav` (no existing wayfarer WAV reference to
supersede — `reference_m6.ppm` needed no changes at all).
