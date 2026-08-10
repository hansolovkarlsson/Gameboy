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

## Status: Milestone 4 (health, HUD, contact damage, heart pickup)

Milestones 1-2 built a small 2x2 grid of bordered rooms with a
freely-walking, collision-checked player and instant edge-to-edge room
transitions. Milestone 3 added combat: `src/sword.c`'s one-shot sword
swing and `src/enemy.c`'s one patrolling enemy (confined to room
(0,0)), a hit permanently defeating it — but the enemy couldn't hurt
the player back yet.

This pass closes that gap, scoped deliberately tight: a full inventory
system has no real use yet (no locked doors exist for a key to open —
that's a later milestone's own stretch scope), so this is player
health, a HUD, contact damage, and exactly one real, immediately-useful
item, not a placeholder. `src/player.c` gained 3 hearts and a brief
(~1s) invincibility window after any hit, so one grazing pass by the
enemy only ever costs one heart. `src/heart_hud.c` is a 3-sprite,
fixed-screen-position HUD (not BG tiles — avoids touching the
already-locked-in room/wall geometry at all) — one hand-drawn heart
tile, shown full or empty via an OBJ-palette swap rather than a second
bitmap. The enemy now damages the player on contact
(`src/world.c`); reaching 0 hearts auto-respawns immediately in room
(0,0), no button press needed. `src/pickup.c` is one heart pickup,
fixed in room (1,1), that fully heals on contact and stays collected
for the session. No knockback and no invincibility-flicker feedback
this pass — real, deliberate scope decisions, not silently missing.

Verified via a scripted `--input` sequence (`input_script_m4.txt`):
standing in the enemy's patrol lane costs exactly one heart (confirmed
via the HUD, and confirmed the *same* pass doesn't cost a second one),
then crossing into room (1,1) and collecting the pickup visibly heals
back to full. Respawn-on-zero-hearts was verified directly (not via a
throwaway build) — the same patrol lane, given enough time, produces
three real hits and a real respawn, observed frame-by-frame. Locked in
as `reference_m4.ppm` (supersedes `reference_m3.ppm`, deleted once the
new one was confirmed).
