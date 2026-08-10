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

## Status: Milestone 5 (scoring, move budget, game-over/restart flow)

The first genuinely *complete* playable build — a score, a finite move
budget, a game-over state once it runs out, and `Start` to restart.
Milestones 1-4 (toolchain bring-up, static grid rendering, cursor +
input, swap/match/clear/gravity/refill) are done.

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

See `docs/GAMEBOY_ROADMAP.md` for Milestone 6 (stretch/polish: sound
effects, SRAM high-score persistence, a real title screen).
