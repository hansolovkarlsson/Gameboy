---
title: "4. The PPU"
nav_order: 5
---

# The PPU

The Picture Processing Unit is what turns your project from a headless
CPU exerciser into a Game Boy. It's also the subsystem with the widest
gap between "draws something" and "draws it correctly" — plan to
return here more than once.

## How the Game Boy makes pixels

No framebuffer. The PPU composes each frame live from three layers
of **8×8 tiles**:

- **Tile data** (`8000–97FF`): 2 bits per pixel, 16 bytes per tile.
  Each row is two bytes; bit *n* of each byte pairs up to form pixel
  *n*'s 2-bit color index (Pan Docs,
  ["Tile Data"](https://gbdev.io/pandocs/Tile_Data.html)).
- **Two 32×32 tile maps** (`9800–9BFF`, `9C00–9FFF`): grids of tile
  indices. The **background** scrolls over one via `SCX`/`SCY`; the
  **window** is a second, non-scrolling layer with its own position.
- **Objects (sprites)**: up to 40 in OAM (`FE00–FE9F`), 4 bytes each
  (Y, X, tile, attributes), 8×8 or 8×16, max **10 per scanline**.

One genuinely confusing quirk to get right the first time: there are
two tile-*addressing* modes (LCDC bit 4). In the `8000` mode tile
indices are unsigned; in the `8800` mode they're **signed** offsets
around `9000`. Games switch between them freely.

Palettes on DMG are tiny: `BGP`/`OBP0`/`OBP1` each map the four 2-bit
color indices to four gray shades. Sprite color 0 is transparent.

## The mode state machine

The PPU cycles through four modes per scanline — Mode 2 (OAM scan,
80 dots), Mode 3 (drawing, ~172–289 dots), Mode 0 (HBlank, the
remainder of the 456-dot line) — for 144 visible lines, then 10 lines
of Mode 1 (VBlank). 154 lines × 456 dots = 70224 dots per frame at
~59.7 Hz (Pan Docs, ["Rendering"](https://gbdev.io/pandocs/Rendering.html)).

Implement this as a real state machine driven by the cycle count your
CPU reports, exposing `LY` (current line), the mode bits in `STAT`,
and the `LY==LYC` compare flag. Games synchronize to these
constantly — the state machine's *timing* is game-visible behavior,
not internal bookkeeping.

For rendering itself, start with **a scanline renderer**: when Mode 3
completes, compute that line's 160 pixels in one pass — background,
then window, then sprites with priority rules. Real hardware pushes
pixels through a FIFO one dot at a time, and some effects can only be
reproduced that way — but a scanline renderer gets you dmg-acid2-level
correctness and is enormously simpler. The reference emulator still
uses one.

Two simplifications the reference project made here, both documented
in-line at the exact code they apply to — and both later replaced when
specific test ROMs demanded it:

- Mode 3 always 172 dots (real hardware varies 172–289 with scroll,
  window, and sprite penalties) — affects STAT interrupt timing, not
  pixel content.
- OAM DMA (the `FF46` register games use to copy sprite data) as an
  instant 160-byte copy rather than a timed 160-M-cycle transfer.

This is the pattern to copy: simplify *consciously*, write it down,
and let test ROMs tell you when the simplification's bill comes due
([chapter 8](08-testing.md) is the story of paying it).

## The correctness gate: dmg-acid2

[**dmg-acid2**](https://github.com/mattcurrie/dmg-acid2) (MIT
licensed, safe to commit) is the Game Boy community's PPU acceptance
test, in the spirit of the browser Acid2 test: it renders a face, and
every feature of the face exercises a specific PPU behavior, with a
known-good reference image to compare pixel-for-pixel. Wire it into
your build as an automated comparison — render a frame with your
`--ppm` flag, compare against the reference, print a match
percentage. (A ~60-line dependency-free PNG decoder is enough; the
reference project's
[`tests/compare_frame.py`](https://github.com/hansolovkarlsson/Gameboy/blob/main/tests/compare_frame.py)
shows the shape.)

Now, the most instructive result in this whole guide. At this point in
the build — PPU done, interrupts not yet — the reference emulator
scored **91.31%**. Rather than chasing pixels, the project *diagnosed
the gap and predicted the fix*: dmg-acid2's README says it uses
`LY==LYC` STAT interrupts to perform register writes on specific
screen rows — mid-frame raster effects. Nearly every failing feature
(the blank footer text, the wrong eyes) was gated behind an interrupt
actually *firing and being handled*, and interrupt dispatch didn't
exist yet. The static parts all rendered correctly.

When interrupt dispatch landed in the next chapter's work, the score
jumped to **98.04% in one step** — strong confirmation the diagnosis
was right, not a guess that happened to sound plausible. The remaining
~2% took the project months of test-driven timing work to close (the
full arc is in [chapter 8](08-testing.md) — it ended at a clean 100%,
via a fix nobody would have predicted here).

The lesson for your build: **when a test fails, produce a specific,
falsifiable explanation before writing a fix.** A percentage that
jumps exactly when predicted teaches you more than ten point-patches.

And a note on baselines: treat that 91.31% as a **floor to regress
against, not a target to decorate**. The reference project's compare
script fails the build if the rate ever drops below the recorded
baseline — so PPU work elsewhere can't silently break what already
worked.

Next: [interrupts, the timer, and the joypad](05-interrupts-timer-joypad.md) —
the machinery that turned 91% into 98%.
