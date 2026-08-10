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

## Status: Milestone 5 (locked door + key)

Milestones 1-4 built a small 2x2 grid of bordered rooms with a
freely-walking, collision-checked, combat-capable player: a one-shot
sword swing, one patrolling enemy confined to room (0,0), 3 hearts with
a sprite-based HUD, enemy contact damage with brief invincibility, and
one heart pickup in room (1,1) that heals.

The remaining roadmap item bundles five independent stretch features;
asked the user which to build first — **locked door + key**, the first
real use for an actual inventory item. A locked door only means
something if it actually gates progress: the plain 2x2 grid used by
Milestones 1-4 is a full 4-room cycle, so locking just one of its four
edges would leave the other three as a free bypass. This pass also
permanently severs the (0,0)↔(0,1) edge (never open, not key-gated —
just different level geometry), turning the map into a genuine
dead-end chain: `(0,0)` [start] → `(1,0)` [**new**: the key] → `(1,1)`
[heart pickup] → **locked door** → `(0,1)` [**new**: otherwise
unreachable].

`src/room.c` gained a third tile — a hand-drawn gold "door" texture
with a dark center seam, distinct from the plain stone wall, so a
locked door reads as *something that opens* rather than just more
wall. Collision needed **no changes at all**: a locked door is drawn
because that side's `has_*` flag is 0, and `room_blocks()` already
enforces closed sides purely from that flag (Milestone 2's own logic,
completely unchanged) — once the key flips the flag to 1, the door is
already passable for free, the same way any other open edge already
was. `src/key.c` is a near-twin of `src/pickup.c` (collect on contact,
stays collected) rather than a shared "collectible" abstraction — the
two behave differently enough, and there are only two of them, that
sharing a base would be premature machinery. Collecting the key has no
immediate visible effect beyond the door itself — no key HUD icon this
pass, a real, deliberate scope decision stated plainly.

Verified via a scripted `--input` sequence (`input_script_m5.txt`):
walking through room (1,0) collects the key, the door in room (1,1)
was separately confirmed to render with the door texture and block
movement while locked, and — with the key collected — walking into
that same door now transitions into room (0,1), reachable no other
way. Locked in as `reference_m5.ppm` (supersedes `reference_m4.ppm`,
deleted once the new one was confirmed — room (0,0)'s own boot-frame
wall pattern changed regardless of the new feature, since its south
side is now permanently walled).
