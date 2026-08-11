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

## Status: Milestone 11 (restart from the win screen)

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
grew the map to a 3x2 grid with an explorable loop.

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
