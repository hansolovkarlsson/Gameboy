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

## Status: Milestone 9 (SRAM save) — roadmap stretch list complete

Milestones 1-4 built a small 2x2 grid of bordered rooms with a
freely-walking, collision-checked, combat-capable player: a one-shot
sword swing, one patrolling enemy confined to room (0,0), 3 hearts with
a sprite-based HUD, enemy contact damage with brief invincibility, and
one heart pickup in room (1,1) that heals. Milestone 5 made room (0,1)
a real, otherwise-unreachable goal room, gated behind a locked door
that a key (in room (1,0)) unlocks. Milestone 6 added a real win
condition — defeat the enemy *and* reach room (0,1) for a one-shot
"WIN" screen. Milestone 7 added five sound effects. Milestone 8 added
a "WAYFARER" / "PRESS START" title screen.

This pass reuses this emulator's own already-proven battery-backed
SRAM persistence (`src/cart.c`'s `gb_cart_load_ram_file()`/
`gb_cart_save_ram_file()`, the CLI's `--sav` flag) and the sibling
`prism/` project's own `highscore.c` register-level pattern — **no
emulator core changes at all**, purely a `wayfarer/` build-config +
game-code addition. `wayfarer/Makefile` gained the same
`-Wl-yt0x03`/`-Wm-ya1` (MBC1+RAM+BATTERY) flags `prism/Makefile`
already uses.

**Scope decision**: only the 4 permanent, one-way progress flags
persist — key collected, heart pickup collected, enemy defeated, game
won — not transient state (player position, current room, current
hearts). A fresh boot always starts at room (0,0) with full hearts; a
loaded save just means already-collected items stay collected, and if
the game was already won, the win screen shows again immediately after
the title screen. New `src/sram.c`/`sram.h` packs all 4 flags into a
single bitmask byte (plus a magic byte) — 2 bytes of real cartridge
SRAM total. `key.c`/`pickup.c`/`enemy.c` each gained a small
`_load_*()` setter that restores state directly, bypassing the normal
collect/hit AABB checks entirely (a state load, not a gameplay event —
no sound effect fires for it).

**A real, unanticipated finding**: the plan assumed a build-config-only
change couldn't affect existing timing — true at the video-frame level
(`reference_m8.ppm` needed no changes, confirmed by direct `cmp`), but
*not* at the audio-sample level. The new SRAM instructions (`ENABLE_RAM`/
`DISABLE_RAM` and the extra function calls) shift exactly *when* each
sound trigger's register writes land relative to the audio sample
clock by a few samples, without moving any video frame — the same
"real code changes shift exact sample position, not frame timing"
category of finding `prism/`'s own history already established more
than once. `reference_m8_sfx.wav` needed re-locking in place (same
filename, same 4 events at the same approximate timestamps, confirmed
via the same programmatic `wave`-module check Milestone 7 used).

Verified the same way `prism/`'s own Milestone 6c was: two real,
separate process invocations sharing one `.sav` file. Running
`input_script_m8.txt` with `--sav` produces a save reflecting exactly
what that script's own known path does (key collected, enemy defeated,
won — it doesn't walk over the heart, so that flag honestly stays 0) —
locked in as `reference_m8.sav`. Loading that save fresh with just a
`START` press (`input_script_m9_start.txt`) shows the win screen
immediately, confirmed stable — locked in as `reference_m9_won.ppm`.
The other 3 flags loading correctly *individually* (not just the
all-true case) was verified once via a separate, uncommitted probe —
collect only the key, save, reload, confirm it's already absent
without re-scripting collection — the same asymmetric-verification
precedent this project has used repeatedly.
