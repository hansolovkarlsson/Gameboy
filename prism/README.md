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

## Status: Milestone 6c (real SRAM high-score persistence)

The best score ever reached, shown on the title screen and genuinely
persisted across separate play sessions via real cartridge SRAM — not
just this milestone's own feature, but a real capability this
project's emulator core didn't have at all before now (see
`docs/GAMEBOY_ROADMAP.md`'s own entry on `src/cart.c`'s new
`gb_cart_load_ram_file()`/`gb_cart_save_ram_file()`). On top of
Milestone 6b's title screen, Milestone 6a's sound effects, and
Milestone 5's complete playable build (score, move budget, game-over,
restart). Milestones 1-6b are done.

`prism/Makefile` now builds a real MBC1+RAM+BATTERY cartridge (`-Wl-yt0x03
-Wm-ya1`, cartridge type `0x03`, 1 SRAM bank) instead of `mbc=none` -
confirmed by checking `bin/gameboy`'s own startup log after building,
not just trusted from the flag docs. New `src/highscore.c`/`highscore.h`:
3 bytes of real SRAM (`gb/hardware.h`'s `_SRAM[]` array, `ENABLE_RAM`/
`DISABLE_RAM`-gated) - a 1-byte magic value plus the score as two
explicit little-endian bytes. A magic-byte mismatch (including a
genuinely fresh cart) means "uninitialized," reset to 0 - never trust
SRAM content blindly, the real-hardware-accurate practice.
`highscore_maybe_update(score)` is called after *every* scoring swap,
not just at game-over - more crash-resilient (data is safe the moment
it's written), and it's also what makes a single ordinary match-clearing
swap (score 0 → 30, which beats the initial 0) enough to exercise a
real SRAM write in the committed regression test, without needing a
full playthrough. `title_screen()` (`title.c`) gained a third line,
"HIGH" plus the 4-digit value below it - two new letter glyphs (H, G,
tile IDs `24-25`) plus the number reusing `hud.c`'s exact digit bitmap
data (now exported via `hud.h`) at a fresh tile-ID range, avoiding art
duplication.

**A real bug found and fixed along the way**: adding the "HIGH <score>"
line (row 15, just below the 12x12 grid's own rows 3-14) exposed that
`board.c`'s `fill_screen_blank()` only ever reset background *tile
IDs* for the whole 32x32 map, never CGB *attributes* (palette index) -
harmless before now, since nothing had ever set a non-zero attribute
outside the grid before `board_init()` ran. Once `title_screen()`
started drawing text there in its own dark-navy `TITLE_PALETTE`, those
cells kept that stale palette assignment into actual gameplay (a
visibly darker patch under the grid), since `board_redraw()` only
re-attributes the grid's own 12x12 region, not row 15. Found by
comparing the new build's captured frame against the still-valid
Milestone 6b reference pixel-by-pixel (not just eyeballing "looks
wrong") - the diff was exactly 32×8 pixels at tile row 15, columns 8-11,
pinpointing the cause immediately. Fixed by having `fill_screen_blank()`
reset attributes to 0 for the whole map too, the same discipline that
already existed for tile IDs.

New `src/title.c`/`title.h`: a small custom letter tileset (same
reasoning as `hud.c`'s digits — not GBDK's console/font system, to
avoid tile-ID collision), just the 8 distinct letters actually needed
across "PRISM" and "PRESS START" (P, R, I, S, M, E, T, A) plus a blank
tile, tile IDs `15-23` right after `hud.c`'s digit range. Each
letterform is the standard 5x7 dot-matrix block-letter shape, rendered
and read back (`Read`-ing the converted PNG) to confirm both lines are
actually legible before locking anything in — text is exactly the kind
of thing this project's "look at it before asserting correctness"
precedent (`gems.c`, `dmg-acid2`) exists for. `title_screen()` shows
the screen and blocks on an edge-triggered `Start` press, then
`wait_vbl_done()`+`DISPLAY_OFF` before returning — a clean,
real-hardware-safe handoff so the tilemap is never caught pointing at
a title-tile ID mid-overwrite with fresh gem/digit bitmap data.

Two design points worth calling out explicitly: `initrand(DIV_REG)`
stays the literal first line of `main()`, with `title_screen()` called
*after* it — the title screen's wait-for-Start loop is the first
genuinely player-paced (unbounded real time) delay in Prism's boot
sequence, and the RNG seed has to be sampled before any such wait or
the random board would depend on how long the player lingered on the
title screen. And the main gameplay loop's `prev_joy` is now seeded
from the real `joypad()` state at loop entry, not hardcoded `0` — a
player still physically holding Start (the same button that just
dismissed the title screen) would otherwise read as a fresh edge on the
very first iteration and silently trigger an extra `restart_game()`.

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
(`--wav`, `prism/reference_m6c_sfx.wav`) against the same scripted input
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

`reference_m6c_sfx.wav` supersedes Milestone 6a/6b's own WAV reference
for a real, by-now-familiar reason: adding `highscore_init()`'s few
extra pre-title-screen instructions shifted every subsequent event's
*exact* real-time/sample position by a small, consistent amount
(~40ms) - not enough to shift which vblank *frame number* any scripted
input event lands on (so the visual `.ppm` reference and game logic are
completely unaffected), but enough to fail a byte-exact `cmp` against
audio captured before that shift existed. The same category of thing
Milestone 5's own RNG-seed-timing discovery already established -
expected, and handled the same way: re-captured, verified (the same
zero-crossing frequency analysis confirmed both tones' actual content
was identical, just time-shifted), and superseded.

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
(`--input prism/input_script_m6b.txt`, which presses Start to dismiss
the title screen first, then the same relative moves as Milestone 5's
own script) that produces a genuine match-creating swap against the
build's deterministic initial board, verified programmatically (not by
eye): the resulting frame's score/moves digit tiles are sampled back
and matched exact-pixel against the known digit bitmaps (`"0030"`/
`"19"`), and the rest of the grid is confirmed to match the expected
compaction/refill with zero remaining matches anywhere. Confirmed the
title screen change didn't perturb the deterministic board at all — the
Milestone 6b capture is byte-identical to the old, pre-title-screen
Milestone 5 reference frame, exactly as the `initrand(DIV_REG)`
placement above was designed to guarantee. `cmp`s against a committed
reference (`prism/reference_m6b.ppm`). Game-over and restart were verified
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

**SRAM persistence itself is verified with two real, separate process
invocations sharing one `.sav` file** — a genuinely stronger test than
"logic looks right," since it exercises the exact same `--sav`
load/save path a real player's session would: run 1 (`--input
input_script_m6b.txt --sav <out>.sav`) starts fresh (no prior save),
makes the same match-clearing swap as always, and its resulting `.sav`
is `cmp`'d against a committed `prism/reference_m6c.sav` (a mostly-zero
8 KiB file — magic `0x48` + score `30` as bytes `0x1E,0x00`, the rest
genuinely zero, matching what a real cartridge's unused SRAM would also
show). Run 2 (`--sav <out>.sav`, no `--input` at all) boots straight to
the title screen using *that exact file* and confirms it now reads
"HIGH 0030" instead of "HIGH 0000", `cmp`'d against a committed
`prism/reference_m6c_title.ppm`.

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
consistently correct. `make gameboy-prism-build` uses `--frames 110`
for generous margin, not because that specific number is meaningful.

Not root-caused to an exact line yet, and deliberately not chased
further as part of this rendering milestone — flagged here honestly
(matching this project's own standing practice, e.g. `dmg-acid2`'s
still-open small gap) as a real, valuable finding for a future,
dedicated investigation into `src/ppu.c`'s LCD-enable/frame-timing
path, not something to silently work around without saying so.

Re-checked for Milestones 3, 4, 5, 6b, and 6c, since each one's game
loop is genuinely dynamic in a new way (Milestone 3: real per-frame
input; Milestone 4: a multi-step cascade resolving over an unknown
number of internal steps; Milestone 5: HUD redraws triggered by that
same cascade outcome; Milestone 6b: the new title-screen-to-gameplay
transition, including its `DISPLAY_OFF`/`DISPLAY_ON` toggle; Milestone
6c: a second, independent boot loading real SRAM content): each
milestone's own scripted `--input` sequence captured at two different
frame counts past its last input event produced byte-identical output
every time, confirming the render is stable well before
`gameboy-prism-build`'s chosen frame count, not coincidentally stable
at exactly one number.

Milestone 6 (stretch/polish) is now complete: sound effects (6a), a
real title screen (6b), and SRAM high-score persistence (6c). See
`docs/GAMEBOY_ROADMAP.md`'s Phase 10 entry for the full writeup and
whatever comes next.
