# RGBDS

The chosen toolchain for any future custom Game Boy test content -
`brew install rgbds` (or your platform's equivalent), version 1.0.3 at
the time this was set up. Not vendored or built from source here; it's
a real, independent, third-party project
(<https://github.com/gbdev/rgbds>), used the same way any of the real
homebrew ROMs already committed under `test_roms/` were built.

**Why RGBDS instead of extending the sibling Z80/CP-M repo's own
`z80asm` (`cpm/asm/src/`)**: considered and rejected. `z80asm`'s macro/
preprocessing, expression evaluator, symbol table, and generic
directives are already CPU-agnostic - only its instruction encoder
(`cpm/asm/src/encode.c`) is Z80-specific, so a `CPU Z80`/`CPU GB`
directive selecting between two encoders while sharing the rest was a
real, well-scoped option, genuinely smaller than the CPU-*emulator*
sharing this project already declined (see
`docs/GAMEBOY_ROADMAP.md`'s "Architecture decision" section - the
SM83 diverges too much from the Z80 at the dispatch/ALU level for that
one to have been worth it). But RGBDS is already the de facto standard
the entire real Game Boy homebrew scene uses - `2048-gb`, `Tobu Tobu
Girl`, and `Droneboy` (`test_roms/`) are all built with RGBDS or
GBDK (which itself builds on RGBDS's assembler) - so adopting it costs
nothing, against real ongoing effort maintaining a second instruction
set inside `z80asm`. Writing test content is a means to an end for this
project (exercising the emulator), not its own mission, unlike the Z80
assembler itself.

**`examples/hello.asm`**: the smallest possible real program, proving
the round-trip (`rgbasm` → `rgblink` → `rgbfix` → this project's own
`bin/gameboy`) actually works, not just that RGBDS itself does - it
emits `"HELLO GAMEBOY"` one character at a time over the serial port
(`SB`/`SC`, `$FF01`/`$FF02`), the same internal-clock-transfer
convention Blargg's own test ROMs use and `src/mmu.c`'s serial
hook already captures. `make gameboy-rgbds-test` assembles, links,
fixes, runs it, and greps the output for that exact string - a real
regression check, not just "the build didn't fail".

**`examples/mbc3_rtc.asm`**: the first real payoff of having RGBDS at
all - a test ROM built specifically to close a gap
`docs/GAMEBOY_ROADMAP.md`'s Phase 6 status had flagged ("particularly
ones exercising MBC3's RTC or deeper save-RAM behavior, neither
meaningfully exercised by 2048-gb"). Drives the real memory-mapped
MBC3 interface directly (bank-select writes at `$4000`-`$5FFF`, the
latch sequence at `$6000`-`$7FFF`, the shared `$A000`-`$BFFF` window) -
a genuinely different, real-hardware-shaped way of exercising the same
logic `tests/test_cart.c`'s synthetic `GBCart`-struct checks
already cover, not a duplicate of them. Writes distinct sentinel bytes
into two banked-RAM banks and all five RTC registers, latches, reads
the latch back, overwrites the *live* registers without re-latching
(confirming the latch stays frozen), then re-latches and confirms the
fresh values now show through - proving latch/live isolation and
banked-RAM isolation both hold, end to end, through real CPU-executed
code. `make gameboy-rgbds-mbc3-test` checks the exact expected output
line. Deliberately scoped to what `cart.c`'s own comment says is
actually implemented: the RTC registers don't yet advance with real
elapsed time, so this ROM tests write/latch/read fidelity, not "does
time actually pass".

**`examples/hdma.asm`**: real-hardware-shaped validation for CGB
HDMA/GDMA (`0xFF51-0xFF55`), closing a gap the Phase 9 HDMA/GDMA
follow-up left open - `tests/test_cpu.c`'s direct unit tests call
`gb_hdma_hblank_trigger()` synthetically rather than through real
CPU-executed code and real PPU timing. A search for a real,
permissively-licensed game or demo that exercises HDMA came up empty
(`tobutobugirl-dx`, already used as a local demo, confirmed via direct
source search to never reference `HDMA5` at all; a real candidate,
`mills32/Parallax-effect-for-Game-Boy-Color`, genuinely does but has no
license and needs a toolchain - SDCC/GBDK - this project hasn't set
up) - see `docs/GAMEBOY_ROADMAP.md`'s HDMA/GDMA follow-up entry for the
full search. Three rounds, each proving something the synthetic unit
tests can't: General-Purpose DMA (the CPU genuinely blocks on the very
next instruction until the transfer completes, no explicit wait needed
in the ROM itself); HBlank DMA (explicitly polls `HDMA5` to completion,
which depends on two *real* HBlank periods actually occurring - i.e.
`ppu.c`'s own Mode 3->0 transition firing `gb_hdma_hblank_trigger()`,
not a direct test call); and VRAM bank isolation (GDMAs into VRAM bank
1 at the same address round 1 used, then confirms bank 0's original
bytes are untouched). `make gameboy-rgbds-hdma-test` checks the exact
expected output line, same as the other two examples' targets - the
ROM's real output was captured first, then locked in as the expected
string, not guessed. Needs `--mode cgb` at runtime, since HDMA is
CGB-only and this ROM's own header doesn't carry the CGB flag.

All three example ROMs are opt-in, same external-dependency reasoning as
`make gameboy-sdl`: never part of plain `make`/`make gameboy-test`, so
the default build stays free of the RGBDS dependency for anyone who
doesn't have it installed.
