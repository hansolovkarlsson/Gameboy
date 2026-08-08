# Mooneye GB Test Suite (Tier 1 + Tier 2's mbc1/mbc5)

Prebuilt binary ROMs fetched from
<https://gekkio.fi/files/mooneye-test-suite/> (build
`mts-20260714-0944-31510e1`, linked directly from the project's own
README as "automatically built and deployed whenever there's new
changes in the main branch") - the real upstream project is
<https://github.com/Gekkio/mooneye-test-suite>, MIT-licensed
(`LICENSE`, Copyright (c) 2014-2022 Joonas Javanainen), confirmed via
`gh api repos/Gekkio/mooneye-test-suite` before committing, same bar
`dmg-acid2`/`2048-gb`/`droneboy`/`tobutobugirl` were already held to.

**Correction to `docs/GAMEBOY_ROADMAP.md`'s Phase 1 note**: that note
said Mooneye "ships as assembly source needing `rgbds` to build" -
checked against the real upstream `Makefile` while scoping this, and
that's wrong: it builds with WLA-DX (`wla-gb`/`wlalink`), a completely
different assembler, not RGBDS. Rather than adding a second assembler
dependency this project doesn't otherwise need, these are the
project's own prebuilt binaries (same distribution channel its CI
publishes to), committed the same way `dmg-acid2`/`2048-gb`/`droneboy`/
`tobutobugirl` already are - no build step, no new toolchain.

## Why only a subset

Mooneye's full suite (`acceptance/`, `emulator-only/`, `manual-only/`,
`madness/`, `misc/`, `utils/`) is far broader than what applies here.
Committed subset: Tier 1 (`acceptance/timer/` (13), `acceptance/bits/`
(3), `acceptance/interrupts/ie_push.gb`, `acceptance/instr/daa.gb`, and
26 top-level CPU/interrupt-timing ROMs - 44 total) plus a Tier 2 slice
added in a follow-up pass, `emulator-only/mbc1/` and
`emulator-only/mbc5/` (21 ROMs) - 65 total.

**Deliberately excluded**:
- `boot_regs-*`/`boot_div-*`/`boot_hwio-*`/`serial/boot_sclk_align-*` -
  this emulator's `gb_cpu_reset()` (`cpu.c`) hardcodes real
  post-boot-ROM register state directly rather than executing an actual
  boot ROM (Nintendo's own copyrighted binary, correctly never dumped
  or committed here - see `README.md`'s cartridge-dump policy). These
  tests measure boot ROM execution specifically, which doesn't apply by
  construction, not a gap to chase.
- `emulator-only/mbc2/` - MBC2 isn't implemented (`cart.c` covers
  MBC-less/MBC1/MBC3/MBC5 only, Phase 2's real scope). Revisit if MBC2
  support is ever added.
- `acceptance/ppu/` (11) + `acceptance/oam_dma*` (6) - highest
  diagnostic value (sits right on the still-open dmg-acid2 pixel-FIFO
  gap and the documented "OAM DMA is instant, not timed" simplification
  - see below), left for a follow-up slice.
- `manual-only/`, `madness/`, `misc/` (CGB/AGB-only), `utils/` -
  explicitly out of scope per Mooneye's own README (visual/audio manual
  verification, non-DMG hardware, non-test utilities).

## Protocol

Mooneye reports its result over the same SB/SC internal-clock serial
transfer Blargg's `cpu_instrs`/`dmg_sound` (used locally, never
committed - see `docs/GAMEBOY_ROADMAP.md`'s Phase 1 licensing note)
already use - `src/mmu.c`'s existing `gb_serial_output_hook` needed no
changes. Per the suite's own README's "Pass/fail reporting" section: a
pass sends the Fibonacci sequence `3,5,8,13,21,34`; a failure sends
`0x42` six times. Every ROM then loops on itself forever - there's no
clean "done" signal beyond that, so `main.c`'s existing fixed
20,000,000-instruction default budget (already used for the RGBDS
tests) is what bounds each run; that's plenty; even the slowest ROM
here finishes signaling in well under a second.

`tests/run_mooneye.py` runs every `.gb` under here through
`bin/gameboy`, checks its raw captured stdout for either byte sequence,
and compares against a real, committed per-ROM baseline - the same
floor-not-target reasoning `tests/compare_frame.py` already uses for
dmg-acid2, just per-ROM instead of a percentage.

## Results: Tier 1 - 24/44 on first run, 28/44 after the small fixes, 41/44 after the OAM-DMA-timing rewrite

Not 20 unrelated mysteries - grounded by reading each failing test's
real `.s` source (`gh api repos/Gekkio/mooneye-test-suite/contents/...`)
rather than guessed at, they trace to five real root causes. Four have
since been fixed in a follow-up pass; see `docs/GAMEBOY_ROADMAP.md`'s
own Mooneye status entries for the full story of both passes:

- **13 of 14 `*_timing` ROMs (FIXED)** - `call_timing`, `call_timing2`,
  `call_cc_timing`/`call_cc_timing2`, `jp_timing`, `jp_cc_timing`,
  `push_timing`, `ret_timing`, `ret_cc_timing`, `reti_timing`,
  `rst_timing`, `ld_hl_sp_e_timing`, `add_sp_e_timing`. All use the
  identical technique: start a real OAM DMA transfer, pad with `nop`s
  tuned so a specific M-cycle of the instruction under test lands
  exactly inside vs. just after the DMA window, then check whether that
  access saw the transfer still in progress. This was a direct, precise
  hit on a gap `ppu.c` used to document from Phase 3: OAM DMA was an
  instant copy, not a real timed 160-M-cycle transfer - correct for the
  universal busy-wait-in-HRAM convention real code uses, wrong for
  exactly this kind of adversarial mid-transfer access these tests are
  built to probe. Closed by a real OAM-DMA-timing rewrite - see "Results:
  the OAM-DMA-timing rewrite - 13/14" below for the full story, including
  the real-hardware reference model it was cross-checked against and the
  one subtle bug (in, of all places, the instruction that *triggers* a
  transfer) that took a second pass to find.
- **`pop_timing.gb` (1 of the original 14, still open, but re-scoped)**:
  turned out, on a full read of its `.s` source rather than just its
  header comment, to not be an OAM DMA test at all - see "Results" below.
- **`if_ie_registers`, `interrupts/ie_push` (FIXED)**: cycle-exact
  behavior when `IE` is written *during* interrupt dispatch's PC push.
  Real hardware doesn't lock in the target vector before either push
  write or after both - it re-reads `IE & IF` right after the
  *high*-byte push specifically, so a write that lands there (SP near
  `$FFFF`/`$0000` at dispatch time) can genuinely cancel or redirect the
  dispatch, while the same clobber on the low-byte write is always too
  late. Fixed in `gb_cpu_step()`'s interrupt-dispatch block and
  `gb_push16()`'s write order (`cpu.c`).
- **`bits/unused_hwio-GS` (FIXED)**: unused/unmapped `$FFxx` bits should
  read back forced to `1`. `P1`/`TAC` already did; `SC` (bits 1-6),
  `IF` (bits 5-7), `STAT` (bit 7), and several fully-unmapped registers
  (`$FF03`, `$FF08`-`$FF0E`, `$FF4C`-`$FF7F`) didn't - fixed in
  `mmu.c`/`ppu.c`.
- **`rapid_di_ei` (FIXED)**: a real, separate EI-delay edge case - `DI`
  immediately after `EI` must never let interrupts turn on even
  momentarily. `gb_cpu_step()`'s "apply EI's delayed enable at the end
  of this step" logic was unconditionally re-applying that enable even
  when this step's own instruction was `DI` (which had just set
  `ime=0`), silently undoing it. Fixed via a new `di_cancels_ei_delay`
  flag `gb_op_di()` sets (`cpu.c`/`cpu.h`).
- **`timer/tima_write_reloading`, `timer/tma_write_reloading`
  (PARTIALLY FIXED)**: pandocs' `Timer_Obscure_Behaviour.md` "TIMA
  reloading" quirk - a TIMA write during the same M-cycle TIMA
  overflowed in cancels the pending reload/interrupt outright; one
  M-cycle later, a TIMA write is ignored but a TMA write propagates
  into TIMA immediately. Implemented in `timer.c` and covered by a
  direct unit test (`tests/test_timer.c`, all 7 new checks pass) - 6 of
  these two ROMs' combined 8 assertions now pass (confirmed by
  rendering each ROM's own on-screen diagnostic via `--ppm`, up from 0
  of 8 before). The remaining 1 assertion each still fails - now
  understood (see `pop_timing.gb` above/below) to be the timer only
  advancing once per whole instruction rather than per real M-cycle,
  the same root cause `pop_timing.gb` has, not further root-caused at
  the time this was written.

## Results: the OAM-DMA-timing rewrite - 13/14

Closing the 14-ROM cluster above needed real, per-M-cycle OAM DMA -
this project's biggest architecture change since the PPU itself.
Before writing anything, cross-checked the exact real-hardware model
(not guessed at, not reverse-engineered from test padding alone) two
ways:

1. **Gekkio's own mooneye-gb** (Rust, `core/src/hardware.rs`'s
   `OamDma`/`emulate_oam_dma`) - the reference emulator this very test
   suite is itself verified against, and the suite's own `.s` sources
   say plainly they're "verified using a flash cartridge with a genuine
   MBC5 chip" for the MBC5 ROMs and equivalent real-hardware runs for
   these - so mooneye-gb's model isn't a guess, it's the ground truth
   this test suite encodes.
2. **`push_timing.s`'s own padding arithmetic** - independently
   recomputing, by hand, exactly which M-cycle its two `push de`
   instructions' high/low-byte writes land on relative to the transfer
   (down to the exact NOP/loop-iteration count), then checking that
   against mooneye-gb's model - they agreed exactly, which is what gave
   confidence to start writing code rather than fumbling with padding
   math until tests happened to pass.

The real model: writing `$FF46` doesn't start a transfer immediately -
real hardware pipelines it through a `requested -> starting -> active`
handoff, one stage advanced per M-cycle, so the first byte doesn't
actually copy until 3 M-cycles after the write. Modeled as
`GBCpu.dma_request_pending`/`dma_starting_pending`/`dma_active`/
`dma_progress` (`cpu.h`), advanced one M-cycle at a time by
`gb_dma_tick()` (`mmu.c`). While active, `gb_read_byte()`/
`gb_write_byte()` (`mmu.c`) make OAM ($FE00-$FE9F) reads return `$FF`
and drop writes entirely - not "whatever DMA happens to be
transferring", a simpler rule than expected, but exactly what
mooneye-gb's own `read()`/`write()` dispatch does, and exactly what
`push_timing.gb`'s own real assertions expect.

The hard part wasn't the DMA state machine itself - it was getting
`cpu.c`'s existing "one `gb_cpu_step()` call = one flat T-state total,
computed once at the end" model to tick `gb_dma_tick()` at the *right*
M-cycle boundaries, not just the right total count. Scoped narrowly
rather than rewritten wholesale: only the opcodes these specific ROMs
probe (`CALL`/`CALL cc`/`RET`/`RET cc`/`RETI`/`RST`/`PUSH rr`/`POP rr`/
`JP nn`/`JP cc`/`ADD SP,e8`/`LD HL,SP+e8`) now call `gb_dma_tick()`
themselves, once per real M-cycle, interleaved with their own reads/
writes/internal-delay cycles - each verified against that opcode's own
M-cycle breakdown straight from the matching `*_timing.s` file's header
comment (`M = 0: instruction decoding`, etc.), not assumed. Every other
opcode still just gets a single lump-sum tick equal to its whole
T-state count once its handler returns - deliberately *not*
per-M-cycle-accurate against DMA, but nothing needs it to be: real code
never touches OAM directly during an active transfer (the same
busy-wait-in-HRAM convention this project's PPU code already relied on
before this rewrite), and no ROM here probes any other opcode's own
timing against DMA. `cpu.c`'s `is_dma_precise_op()` names the exact set
this applies to.

First full run after this landed: still 0/14 fixed, all 14 ROMs
failing identically to before. Root-caused (not re-guessed) by tracing
`push_timing.gb` byte-for-byte against the same padding arithmetic
already cross-checked in step 2 above: the very instruction Mooneye's
own `start_oam_dma` macro uses to trigger a transfer, `ldh (<DMA), a`
(`gb_op_ldh_a8_a`), was itself one of the "every other opcode" lump-sum
handlers - meaning the write to `$FF46` that everything else is
measured relative to was landing 2 M-cycles too early, silently
shifting every downstream NOP-padded test by exactly that much. Adding
`gb_op_ldh_a8_a` to the same precise-ticking set (despite `$FF46` not
being OAM itself, and despite this instruction's own timing not being
what any test directly asserts on) fixed all but one ROM in a single
pass: 13/14.

The 14th, `pop_timing.gb`, doesn't move - because, on a full read of
its `.s` source (not just its header comment, the mistake that lumped
it in with the other 13 in the first place), it was never an OAM DMA
test. It points `SP` at the `DIV` register (`$FF04`) and checks whether
`POP`'s own low/high-byte reads see `DIV`'s own increment depending on
which exact M-cycle they land on - the identical *kind* of per-M-cycle
precision problem, but against the timer, not DMA. This emulator still
advances the timer once per whole instruction (a lump sum in `main.c`'s
run loop), not per real M-cycle, so `POP`'s intra-instruction reads of
`$FF04` can't see a DIV edge that happens strictly *between* two of its
own M-cycles. Left open, honestly re-scoped as a real, distinct gap
(and the same root cause `tima_write_reloading`/`tma_write_reloading`'s
own last unresolved assertion has, above) rather than folded back into
"still needs the DMA rewrite" the way it was before this pass - that
gap is closed now; this one wasn't in scope for it.

A follow-up attempt applied this same "tick the precise opcodes only"
approach to the timer directly and had to be reverted - real,
measurable regressions in 10 previously-passing ROMs, root-caused (not
guessed) to a genuine architectural difference between DMA and the
timer, not a bug in the attempt itself. See
`docs/GAMEBOY_ROADMAP.md`'s "Timer M-cycle precision: attempted,
reverted" status entry for the full story.

Verified the same way every fix in this project's history has been:
full `gameboy-test` (including a new `test_cart.c`-style direct unit
test for `gb_dma_tick()`'s power-on-safe zero-initialization, see
`cpu.h`'s own comment on why `dma_request_pending`/`dma_starting_pending`
are separate present/value pairs rather than a `-1`-sentinel `int`),
`gameboy-visual-test` (98.04%, unchanged), `gameboy-2048-test`,
`gameboy-droneboy-test`, `gameboy-tobu-test`, `gameboy-savestate-test`
(extended to round-trip a genuinely mid-transfer DMA state, not just
CPU/PPU/timer/APU/cart fields that were already covered), and both
RGBDS targets all still pass byte-for-byte identically - a real,
additive correctness improvement with zero observed regressions.

## Results: Tier 2's mbc1/mbc5, 20/21

A follow-up slice fetched the same prebuilt archive's
`emulator-only/mbc1/` and `emulator-only/mbc5/` ROMs (21 total,
`mts-20260714-0944-31510e1` - same build as Tier 1, just a different
subdirectory of the same tarball).

- **All 8 `mbc5/rom_*.gb` (FIXED)**: a real bug, not a test-harness
  quirk. Unlike MBC1/MBC3, MBC5 has no read-time "bank 0 reads as bank
  1" quirk at $4000-7FFF - pandocs' `MBC5.md` is explicit that "writing
  0 will indeed give bank 0 on MBC5, unlike other MBCs" - so
  `gb_cart_load()` (`cart.c`) leaving a fresh MBC5 cart's ROM bank
  register at its `memset`-zeroed 0 meant `$4000-7FFF` showed bank 0's
  content instead of bank 1's, from the moment the ROM loaded. Every
  one of these 8 ROMs calls straight into ROMX-bank library code
  (`memcpy`, in this case) before ever writing the bank register
  itself, so all 8 failed identically - traced with a temporary PC-trace
  instrument (not committed) showing execution landing on bank 0's
  unused-space padding (`$FF` bytes) at the call target and crashing
  into the `$0038` RST trap within ~50 instructions of boot. Real MBC5
  hardware powers up with that register already at 1, not 0 - verified
  against Gekkio's own reference emulator, mooneye-gb
  (`core/src/hardware/cartridge.rs`'s `Mbc5State::default()`,
  `romb0: 0b0000_0001`), itself checked against a real MBC5 flash
  cartridge (this suite's own `rom_*.s` sources: "Results have been
  verified using a flash cartridge with a genuine MBC5 chip"). Fixed
  by setting `rom_bank_lo = 1` for `GB_MBC5` in `gb_cart_load()`
  (`cart.c`); also covered by a new direct unit test
  (`tests/test_cart.c`'s `test_mbc5_default_bank_is_one()`).
- **`mbc1/multicart_rom_8Mb.gb` (not attempted at the time - since
  FIXED, see "Results: MBC1M multicart detection" below)**: a genuinely
  distinct MBC1 hardware variant, not a regular large-ROM MBC1 cart bug.
  MBC1M multi-game compilation carts wire bit 4 of the `$2000-3FFF` ROM
  bank register out entirely (pandocs' `MBC1.md` "MBC1M addressing
  diagrams": "From 2000-3FFF bank register (bit 4 unused)"), so the
  bank-number formula genuinely differs from `cart.c`'s existing MBC1
  handling, which assumes the regular large-ROM wiring. Real detection
  needs its own multicart heuristic (real emulators typically check for
  a valid Nintendo logo repeated at each 256 KiB boundary) plus a
  distinct address decode once detected - a small but genuinely
  separate feature, not a one-line quirk fix, left for a follow-up
  slice rather than guessed at here.
- The other 12 `mbc1/*.gb` ROMs (`bits_bank1`/`bits_bank2`/`bits_mode`/
  `bits_ramg`/`ram_64kb`/`ram_256kb`/`rom_512kb`/`rom_1Mb`/`rom_2Mb`/
  `rom_4Mb`/`rom_8Mb`/`rom_16Mb`) all passed on the first run - real,
  independent confirmation of `cart.c`'s existing MBC1 banking logic
  beyond `tests/test_cart.c`'s own struct-level tests.

See `tests/run_mooneye.py`'s own `EXPECTED` table for the per-ROM
baseline this locks in as a regression floor.

## Results: MBC1M multicart detection - `mbc1/multicart_rom_8Mb.gb` fixed, 62/65

A later follow-up slice picked up the one deferred ROM from the section
above. This ROM's own `.s` source is explicit that "MBC1 multicarts
*cannot* be detected from the header alone" - a real MBC1M cart's
header is indistinguishable from a regular large-ROM MBC1 cart's - so
a real fix needs both a detection heuristic and a distinct address
decode, not a one-line quirk.

pandocs' `MBC1.md` "MBC1M" section documents the real wiring
difference precisely: the secondary 2-bit register lands on ROM-bank
bits 4-5 instead of the usual 5-6, and the primary 5-bit register is
truncated to its low 4 bits for banking - but the *full* 5-bit register
still feeds the existing "reads as bank 1, not bank 0" quirk, computed
*before* any multicart truncation (a consequence pandocs states the
inputs for but doesn't spell out directly). Confirmed byte-for-byte
against this ROM's own `expected_banks` table (fetched from
Gekkio/mooneye-test-suite): writing the primary register to 16
(`0b10000`) does *not* trigger the quirk even though its truncated low
nibble is 0 - only a literal 0 does - now covered directly by
`tests/test_cart.c`'s `test_mbc1_multicart_rom_banking()`.

For detection, pandocs only documents the identifying trait itself ("a
Nintendo copyright header in bank $10"), not a precise algorithm, so
`is_mbc1_multicart()` (`cart.c`) is a direct port of Gekkio's own
mooneye-gb (`core/src/config/cartridge.rs`) - the same reference
already cross-checked elsewhere in this file, and the concrete
implementation this ROM's own comment ("this triggers heuristics in
some emulators (e.g. mooneye-gb)") was written to satisfy: only a real
1 MiB ROM ("only 8 Mbit MBC1 multicarts exist", per both pandocs and
mooneye-gb) with a valid Nintendo logo at 3 or more of its 4 256 KiB
page boundaries counts - tolerating a menu-less layout while not
misfiring on a regular 1 MiB MBC1 game, which only ever has a valid
logo in page 0. Covered by `test_mbc1_multicart_detection()`'s two
synthetic ROMs (one flagged multicart, one correctly not).

The new `mbc1_multicart` field needed no savestate changes - like
`mbc_type`/`rom_banks`, it's fully re-derived from the ROM file at
`gb_cart_load()` time, and `gb_savestate_load()`'s own fingerprint
check already guarantees the same ROM is loaded first.

Zero regressions across the full existing suite. `EXPECTED` in
`tests/run_mooneye.py` updated to `PASS` for this ROM - **62/65** on
the committed Mooneye subset, up from 61/65.
