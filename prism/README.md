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

## Status: Milestone 7 (animated gem swap)

The best score ever reached, shown on the title screen and genuinely
persisted across separate play sessions via real cartridge SRAM — not
just this milestone's own feature, but a real capability this
project's emulator core didn't have at all before now (see
`docs/GAMEBOY_ROADMAP.md`'s own entry on `src/cart.c`'s new
`gb_cart_load_ram_file()`/`gb_cart_save_ram_file()`). On top of
Milestone 6b's title screen, Milestone 6a's sound effects, and
Milestone 5's complete playable build (score, move budget, game-over,
restart). Milestones 1-6c are done; see below for Milestone 7
(animated gem swap).

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

### Corrected finding: the "PPU rendering-catch-up quirk" was a misdiagnosis

Every milestone since Milestone 2 carried an open note here about a
"PPU timing bug": capturing a frame too soon after `DISPLAY_ON` could
show stale or torn content, and it was flagged as unresolved emulator
behavior worth a future `src/ppu.c` investigation. Investigated
properly (user-requested) and **it was never an emulator bug** — real
CGB hardware's boot ROM leaves the LCD *on* (`LCDC=$91`) and every
background palette color *white* before handing off control to the
cartridge (pandocs' `Power_Up_Sequence.md`, already correctly modeled
in this project's own `src/cpu.c`, `gb_cpu_reset()`). Confirmed via
direct instrumentation (a temporary `fprintf` at every `frame_ready`
event, reverted after use): frames 1-5 were plain white (the real
boot-ROM state - LCD on, VRAM/palette still blank), frame 6 was a
genuinely torn frame captured *mid-write*, squarely inside
`title_screen()`'s own `set_bkg_data`/`set_bkg_tiles`/`set_bkg_palette`
calls, and frame 7 onward was stable once those writes finished.

The real cause: the LCD is live from the moment control passes to the
game - never explicitly turned off - and `title_screen()` (like every
earlier milestone's own initial `gems.c`/`hud.c`/`board.c` setup)
wrote directly to VRAM/tilemap/palettes without first disabling the
display. Real hardware would tear exactly the same way under identical
code - a well-known GB homebrew gotcha (real games disable the LCD, or
wait for VBlank, before bulk VRAM writes), not a timing defect in this
emulator. `title_screen()` already got this right for its own
title→game *transition* (`wait_vbl_done()`+`DISPLAY_OFF`, at its own
end) - it just never did it for its own *initial* setup.

**Fixed** by adding the same `wait_vbl_done();DISPLAY_OFF;` pair as
the first two statements of `title_screen()`, before any of its setup
writes. Verified directly, not just reasoned about: frames 1-3 are now
plain white (LCD correctly off during setup) and frame 4 onward is the
*complete, untorn* title screen immediately - no partial/torn frame
ever appears at any captured frame count. A pleasant, unplanned
consequence confirmed while re-verifying: this fix has **zero** effect
on any of Milestone 6a/6b/6c's own committed references except the
audio one - the `.ppm`/`.sav` outputs from the same scripted `--input`
run are still byte-*identical* to the pre-fix references, since
vblank-frame *numbers* (what scripted input events are keyed to) are
completely unaffected once past the title screen's own brief setup
window. Only `reference_m6c_sfx.wav` needed re-locking, for the same
reason Milestone 6c's own `highscore_init()` addition once did: the
added `wait_vbl_done()` shifts every subsequent audio event's exact
real-time/sample position by a small, fixed amount, without shifting
which frame number anything lands on.

Milestone 6 (stretch/polish) is complete: sound effects (6a), a
real title screen (6b), and SRAM high-score persistence (6c).

### Milestone 7: animated gem swap

User-directed: a swap should visibly show the two gems trading places
before a match resolves, not jump straight to the resolved board.
`board_try_swap()` (`board.c`) still owns match-detection/cascade/
redraw exactly as before — the animation wraps around it, not inside
it. Asked the user whether a reverted (non-matching) swap should also
slide back apart visibly, or just snap back as it always had; they
chose to animate both directions for consistent feedback on every
attempt.

New `src/swapanim.c`/`swapanim.h`: `swapanim_play()` blanks the two
cells' background tiles, then slides two 16x16 sprite gems across each
other over 8 frames (2px/frame, an exact divisor of the 16px cell
pitch — no rounding remainder). No new art — it reuses `gems.h`'s own
diamond-quadrant bitmap and palettes, loaded once more into sprite-
tile/OBJ-palette address space (confirmed via `gb/gb.h`'s own
`set_bkg_data()` doc note that sprite and background tiles only share
memory at IDs 128-255 — every ID used here is well below that). It
even reuses a *single* quadrant tile mirrored via `S_FLIPX`/`S_FLIPY`
for all four corners — the same technique `cursor.c`'s corner brackets
already used — after confirming the committed `gem_tiles` bytes are
exact bit-reversals/row-reversals of each other, not four separate
tiles as the background version uses.

`main.c`'s `handle_select()` now peeks both cells' gem types
(`board_peek()`, new) before swapping, hides the cursor and selection
marker for the animation (`cursor_hide()`/`cursor_show()`, new in
`cursor.c` — keeps the concurrently-visible sprite count under the
hardware's 10-per-scanline limit, since `swapanim.c`'s own 8 gem
sprites already use most of that budget on a same-row horizontal
swap), plays the forward slide, then calls the unchanged
`board_try_swap()`. On a match, its existing internal redraw handles
everything. On a revert, `main.c` plays the slide back apart and calls
the newly-exposed `board_redraw()` itself, since `board_try_swap()`
has no reason to redraw when `grid[]` never actually changed.

**Verified past "it compiled"**: traced individual pixel columns
frame-by-frame across a real captured swap — eyeballing zoomed
thumbnails alone made the real 2px/frame motion look deceptively
static — and confirmed both gems migrate monotonically and land
exactly swapped. The revert path's pixel trace initially looked like a
double-trigger bug (an apparent double oscillation); confirmed it was
actually the correct there-and-back shape of one forward+backward
animation by adding a temporary one-shot serial-port counter in
`handle_select()` (reverted after use — the same instrument/observe/
revert discipline the PPU quirk investigation below already
established) that proved the swap branch fires exactly once per
attempt. A one-frame render tear seen while probing arbitrary frame
counts (mid-redraw — the same well-understood risk any bulk VRAM
write during live gameplay already carried; `board_redraw()` already
ran unguarded mid-gameplay on every prior milestone's matching swap,
not a new regression) resolved by the very next frame, confirmed
transient before picking the final reference frame count.

New `input_script_m7.txt` (replaces `input_script_m6b.txt`) adds a
deliberately non-matching leading swap — (0,0) and (1,0), Blue and
Purple on the deterministic board, found by inspecting a captured
frame, not guessed — ahead of the same matching swap the old script
used, exercising both the forward-only and forward-then-back animation
paths in the locked regression reference. Every frame number
re-derived empirically, since each blocking `swapanim_play()` call
inserts up to 16 real frames that didn't exist before this milestone;
confirmed stable from frame 195 onward before locking in
`reference_m7.ppm` (replaces `reference_m6b.ppm`) and
`reference_m7_sfx.wav` (replaces `reference_m6c_sfx.wav` — now 4 real
SFX events: select, revert, select, match). Explicitly re-confirmed
(not assumed) that `reference_m6c.sav` and `reference_m6c_title.ppm`
are unaffected by re-running those exact checks — both still pass
unchanged, since final score/moves state and the separate
title-screen-only invocation are untouched by a purely-rendering
animation. Zero regressions: the full existing suite (unit tests,
Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%, `2048-gb`,
`droneboy`, `tobutobugirl`, savestate round-trip, all three RGBDS
ROMs) stayed green throughout.

See `docs/GAMEBOY_ROADMAP.md`'s Phase 10 entry for the full writeup
and whatever comes next.
