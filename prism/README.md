# Prism (working title)

This project's first original homebrew game — a CGB-only, grid-based
color-matching puzzle. See `docs/GAMEBOY_ROADMAP.md` for the full
concept writeup and milestone roadmap; this file just covers the
toolchain and where things live.

## Toolchain: GBDK-2020

Written in C using [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020),
not RGBDS (`rgbds/` in the repo root) — real game logic is dramatically
faster to write in C than hand-written SM83 assembly, and GBDK's own
`lcc` compiler driver handles compile/assemble/link/binary-conversion
in one pass.

No Homebrew formula exists for GBDK-2020 (`brew search gbdk` finds
nothing). Install by extracting the official prebuilt release archive —
not vendored in this repo, same "real third-party toolchain" treatment
`rgbds/README.md` already established for RGBDS:

```
mkdir -p prism/toolchain
curl -sL -o /tmp/gbdk.tar.gz \
  https://github.com/gbdk-2020/gbdk-2020/releases/download/4.5.0/gbdk-macos-arm64.tar.gz
tar -xzf /tmp/gbdk.tar.gz -C prism/toolchain
```
(`gbdk-macos-arm64.tar.gz` for Apple Silicon Macs — GBDK-2020's
Releases page also ships `gbdk-macos.tar.gz` (Intel), `gbdk-linux64.tar.gz`/
`gbdk-linux-arm64.tar.gz`, and Windows builds.) This leaves
`prism/toolchain/gbdk/bin/lcc` in the path `prism/Makefile` expects by
default (`GBDK_HOME := toolchain/gbdk`) — override `GBDK_HOME` if
installed elsewhere. `prism/toolchain/` and `prism/bin/` are gitignored.

**License**: GBDK-2020 is GPLv2+LE ("linking exception"). Its own
`LICENSE` file answers the question that actually matters here
directly: *"What license information is required when distributing the
compiled ROM (binary) of my game? A: There is no requirement to include
or credit any of the GBDK-2020 licenses or authors."* This is a
different situation from `docs/GAMEBOY_ROADMAP.md`'s still-open
question about committing a *third party's* GPL-licensed game
(`ucity`) as a test asset — here GBDK-2020 is a compiler/library this
game *uses*, and the linking exception means Prism's own source and
compiled ROM carry no GPL obligation at all, the same model real
commercial modern GB homebrew already ships under with GBDK.

## Building

```
make gameboy-prism-build   # from the repo root - opt-in, needs GBDK-2020 installed
```
Builds `prism/bin/prism.gb` (via `make -C prism`) and smoke-runs it
through this project's own `bin/gameboy --mode cgb` (CGB-only game -
`--mode cgb` is needed since `--mode auto` would already pick CGB from
the ROM's own header too, but explicit matches how the RGBDS HDMA test
ROM is run). Opt-in, same as `gameboy-sdl` and the RGBDS targets: never
part of plain `make`/`make gameboy-test`.

## Status: Milestone 6a (sound effects)

Real audio feedback on top of Milestone 5's complete playable build —
a score, a finite move budget, a game-over state once it runs out, and
`Start` to restart. Milestones 1-5 (toolchain bring-up, static grid
rendering, cursor + input, swap/match/clear/gravity/refill, scoring/
move budget/game-over/restart) are done.

GBDK-2020 has no higher-level sound API — new `src/sfx.c`/`sfx.h`
write directly to the DMG/CGB sound registers (`gb/hardware.h`), the
same "raw register pokes, grounded in a real primary source" approach
used everywhere else in this project. Four one-shot triggers, each a
single write-and-restart relying entirely on real hardware envelope/
sweep/length-counter decay (no per-frame service loop): `sfx_play_select()`
(channel 1, a short high blip on selecting a cell), `sfx_play_revert()`
(channel 1, a falling pitch via a real hardware sweep, on a swap that
creates no match), `sfx_play_match()` (channel 1, a rising pitch via
the opposite sweep direction — deliberately the mirror image of
`sfx_play_revert()` so the two are easy to tell apart by ear — on a
swap that clears at least one gem), and `sfx_play_gameover()` (channel
4 noise, a longer, lower buzz decaying via its volume envelope alone —
a different channel and timbre, not just a different pitch, from the
other three, so it reads as a distinct "session over" event). No sound
on `restart_game()` this pass — a deliberate scope decision, not an
oversight: the visual reset is enough feedback on its own.

`make gameboy-prism-build` now `cmp`s a second captured artifact
(`--wav`, `prism/reference_m6_sfx.wav`) against the same scripted input
run used for the visual check, exercising `sfx_play_select()` and
`sfx_play_match()`. Verified before locking in, not just "a file was
produced": a small Python script (the `wave` module, zero-crossing
frequency estimation per 20ms window) confirms genuine silence before
the first scripted input event, a mid-high blip at the select frame,
and a rising tone at the match frame. `sfx_play_revert()` and
`sfx_play_gameover()` aren't reachable from that exact script (it never
attempts an invalid swap, and never runs the move budget to 0), so both
were verified the same way Milestone 5 verified game-over *logic*
itself — a throwaway scratch capture (an extra probe swap for revert;
`STARTING_MOVES` temporarily reduced to 1 for game-over, never
committed at that value) confirmed each has a real, audibly distinct
signature (revert: a clearly lower average frequency than select/match;
game-over: a much longer, low-amplitude, broadband tail, ~720ms,
absent from every other event) before being reverted back out.

No GBDK console/font system — risk of tile-ID collision with `gems.c`'s
own tiles `0-4` in the same VRAM bank. New `src/hud.c`/`hud.h` instead:
a custom digit tileset, ten 8x8 tiles (`0`-`9`, tile IDs `5-14`)
rasterized from the classic 7-segment-display convention. `hud_init()`
loads the tileset; `hud_set_score(uint16_t)`/`hud_set_moves(uint8_t)`
redraw only their own fixed-width digit field, in background row `y=0`
(the grid, `y=3-14`, never touches it).

`board.c`'s `find_matches()` now returns a cell count instead of a
boolean, and `board_try_swap()` returns the total gems cleared across
its whole cascade (`uint16_t`, 0 = reverted). `main.c` owns session
state alongside its existing `selected` state: `score += cleared * 10`
on a committed swap (a reverted swap costs nothing), `STARTING_MOVES`
(20) → 0 enters game-over (further swap attempts no-op; cursor movement
and `Start` still work), and `Start` always restarts — mid-game or
after game-over alike — resetting score/moves and generating a fresh
board.

`make gameboy-prism-build` drives a real scripted input sequence
(`--input prism/input_script_m5.txt`) that produces a genuine
match-creating swap against the build's deterministic initial board,
verified programmatically (not by eye): the resulting frame's score/
moves digit tiles are sampled back and matched exact-pixel against the
known digit bitmaps (`"0030"`/`"19"`), and the rest of the grid is
confirmed to match the expected compaction/refill with zero remaining
matches anywhere. `cmp`s against a committed reference
(`prism/reference_m5.ppm`). Game-over and restart were verified
separately in a throwaway build (`STARTING_MOVES` temporarily reduced
to 1, never committed at that value) — see
`docs/GAMEBOY_ROADMAP.md`'s Phase 10 Milestone 5 entry for the full
writeup, including a real debugging trail: the HUD initially appeared
not to update at all, which two hypotheses (the PPU rendering-catch-up
quirk below, and VRAM Mode 2/3 write blocking) both failed to explain —
the actual cause was that adding `hud.c` shifted this build's exact
pre-`initrand(DIV_REG)` instruction timing, producing a different (but
still fully deterministic) random board than Milestone 4's script
expected, so its hardcoded swap coordinates simply weren't a
match-creating swap on this build's board. Not an emulator or
game-logic bug — a test-script confound, now documented so it isn't
re-discovered from scratch the next time a new source file shifts a
pre-`initrand()` boot path.

### Known emulator finding: a real rendering-catch-up quirk

Building this milestone surfaced a genuine, reproducible timing bug in
this emulator's own PPU — not a bug in Prism's code. Capturing a frame
too soon after `DISPLAY_ON` can render a *stale* pre-write screen (only
the earlier, all-red default tile-map content, or only some scanlines
correctly showing the fresh grid while others still show stale
content), even though the actual VRAM bytes are already fully correct
by that point (confirmed directly via a savestate dump: the tile-map
and CGB-attribute bytes were exactly right, and even the PPU's own
internal `cgb_framebuffer` already held the correct pixel data — only
the frame *captured and written out* was stale). Bisected empirically:
reproducible even with a plain CPU busy-loop before `DISPLAY_ON` (no
VRAM writes at all involved), so it's tied to elapsed pre-display CPU
time in general, not specifically to how many `set_bkg_tiles` calls are
made. For this exact ROM, frame 5 and frame 8 (partially — some
scanlines correct, others not) were stale; frame 10 onward was
consistently correct. `make gameboy-prism-build` uses `--frames 90` for
generous margin, not because that specific number is meaningful.

Not root-caused to an exact line yet, and deliberately not chased
further as part of this rendering milestone — flagged here honestly
(matching this project's own standing practice, e.g. `dmg-acid2`'s
still-open small gap) as a real, valuable finding for a future,
dedicated investigation into `src/ppu.c`'s LCD-enable/frame-timing
path, not something to silently work around without saying so.

Re-checked for Milestones 3, 4, and 5, since each milestone's game loop
is genuinely dynamic in a new way (Milestone 3: real per-frame input;
Milestone 4: a multi-step cascade resolving over an unknown number of
internal steps; Milestone 5: HUD redraws triggered by that same cascade
outcome): each milestone's own scripted `--input` sequence captured at
two different frame counts past its last input event produced
byte-identical output every time, confirming the render is stable well
before `gameboy-prism-build`'s chosen frame count, not coincidentally
stable at exactly one number.

See `docs/GAMEBOY_ROADMAP.md` for the rest of Milestone 6 (stretch/
polish: SRAM high-score persistence, a real title screen).
