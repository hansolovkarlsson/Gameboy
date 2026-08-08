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

## Results: Tier 2's `acceptance/oam_dma*` - 6/6, and two more real gaps in the OAM-DMA-timing rewrite

Fetched from the same prebuilt archive as the other Tier 2 slices
(`mts-20260714-0944-31510e1`) - `acceptance/oam_dma_start.gb`,
`oam_dma_timing.gb`, `oam_dma_restart.gb`, and `acceptance/oam_dma/
basic.gb`/`reg_read.gb`/`sources-GS.gb`, deferred at the original Tier
1 adoption and picked up now that DMA is real and timed. 2 passed
immediately (`basic.gb`, `reg_read.gb`); the other 4 exposed two more
real, distinct gaps in the OAM-DMA-timing rewrite itself.

- **`oam_dma_start.gb`/`oam_dma_timing.gb`/`oam_dma_restart.gb`
  (FIXED)**: `oam_dma_start.gb`'s own `.s` source (fetched in full, not
  guessed at) uses a genuinely clever trick to probe the exact
  DMA-pipeline M-cycle boundary - it self-modifies ROM so a `jp` lands
  on an `LD (HL),A` opcode sitting one byte before OAM, whose execution
  both writes `$FF46` (starting DMA) *and* falls straight through into
  fetching the next opcode from OAM itself, which reads back `$FF`
  (`RST $38`) once DMA has actually gone active, vs. the real `INC B`
  opcode still sitting there if it hasn't. `oam_dma_timing.gb` is more
  direct: it NOP-pads an `LD A,(HL)` read of OAM to land exactly one
  T-state before vs. after DMA's 160th and final copy. Both `LD (HL),A`
  and `LD A,(HL)` - along with every other `LD r,(HL)`/`LD (HL),r`
  opcode, `$40`-`$7F` - are dispatched through one shared handler,
  `gb_op_ld_r_r` (`cpu.c`), which wasn't in `is_dma_precise_op()`'s set,
  so its one real memory access got the same "lump sum after the whole
  instruction" treatment any ordinary opcode gets - exactly the class
  of bug the original OAM-DMA-timing rewrite's own `LDH (a8),A` fix
  (see "Results: the OAM-DMA-timing rewrite" above) already found and
  fixed once, just in a far more commonly-executed opcode this time.
  Fixed identically: tick once immediately before the one real memory
  access (only when either operand is index 6, i.e. `(HL)` - plain
  register moves and HALT touch no memory and don't need it); added
  `gb_op_ld_r_r` to `is_dma_precise_op()`.
- **`oam_dma/sources-GS.gb` (FIXED)**: a wholly different, real
  hardware quirk, not a timing gap. DMA's own address generator has no
  special case for OAM/I-O the way the CPU's normal bus decoder does,
  so a source page of `$E0`-`$FF` (pandocs' `OAM_DMA_Transfer.md`
  documents only `$00`-`$DF` as valid) actually reads WRAM at
  `$C000`-`$DFFF` instead - real hardware's page with bit 5 (`0x20`)
  cleared. Confirmed against Gekkio's own mooneye-gb (`hardware.rs`'s
  `emulate_oam_dma()`: source pages `0xe0..=0xef` and `0xf0..=0xff`
  route to the exact same `work_ram.read_lower()`/`read_upper()` calls
  as `0xc0..=0xcf`/`0xd0..=0xdf`) and against this ROM's own body: it
  sources DMA from page `$FE`/`$FF` and asserts OAM ends up with
  whatever pattern was written to `$DE00`/`$DF00` beforehand - exactly
  what a cleared bit 5 predicts. Fixed in `gb_dma_tick()` (`mmu.c`) by
  masking source pages `>= 0xC0` with `& 0xDF` before forming the
  source address, so the existing `gb_read_byte()` call naturally lands
  on real WRAM.

Covered by `tests/test_cpu.c`'s new `test_dma_wram_mirror_source()`
(three cases: page `$FE`->`$DE00`, page `$FF`->`$DF00`, and a
legitimate page `$C0` left untouched by the masking). Zero regressions
across the full existing suite. All 6 ROMs added to `EXPECTED` as
`PASS` - **68/71** on the committed Mooneye subset, up from 62/65.

## Results: Tier 2's `acceptance/ppu/` - 5/12, a real STAT interrupt model, and a free dmg-acid2 improvement

Fetched from the same prebuilt archive (`mts-20260714-0944-31510e1`) -
12 ROMs, not the 11 earlier status entries estimated before actually
listing the tarball's contents. 2 passed immediately; 5 more needed a
genuine rebuild of how `ppu.c` requests STAT interrupts; the remaining
7 need real per-dot PPU precision this project doesn't have yet.

The old code requested a STAT interrupt unconditionally at every mode
transition whose select bit happened to be set - close to right, but
not what real hardware does. pandocs' `Interrupt_Sources.md` "INT $48
- STAT interrupt" is explicit: the 4 sources (Mode 0/1/2, LYC==LY) are
"logically ORed into a shared STAT interrupt line", and an interrupt
fires only on that line's **rising edge**, not whenever a source's own
condition is merely true - the documented consequence being "STAT
blocking" (same page, citing `stat_irq_blocking.gb` as its own
example): if one source already holds the line high, another source's
condition becoming true produces no new edge, so no second interrupt.
Rebuilt around an explicit `ppu->stat_line` (`ppu.h`) persisted across
calls, recomputed by `update_stat_line()` (`ppu.c`) at every call site
that can change any of the 4 conditions, firing only on a genuine 0->1
transition.

- **`stat_irq_blocking.gb` (FIXED)**: round 1 (enabling Mode 1 select
  while already in VBlank fires an immediate edge) and round 2 (an
  LY==LYC coincidence held continuously through a Mode 3->0 transition
  suppresses Mode 0's own interrupt, since the line never dropped low
  in between) both depend on exactly this model - not achievable with
  the old per-transition-unconditional approach.
- **`stat_lyc_onoff.gb` (FIXED)**: a separate, real quirk found
  alongside the line model - LYC's comparison flag is "constantly
  updated" (pandocs' `STAT.md`), not just at scanline boundaries. The
  old code only recomputed it inside `gb_ppu_step()`'s LY-increment
  paths, so a mid-frame LYC write, or turning the LCD back on (which
  resets LY to 0 and should immediately re-evaluate against it, firing
  a real interrupt if newly true), never recomputed the flag at all.
  Fixed by calling `update_lyc_flag()` (now interrupt-side-effect-free)
  from both `gb_ppu_write_reg()`'s `$FF45` and `$FF40` (the LCD-on
  transition) handlers, each followed by `update_stat_line()`. The
  comparison clock stays deliberately frozen while the LCD is off,
  grounded directly in this ROM's own round-by-round assertions.
- **`vblank_stat_intr-GS.gb` (FIXED)**: a third real quirk - the
  VBlank transition also fires the Mode 2 (OAM) STAT condition, if
  selected, not just Mode 1. Not documented on pandocs' general
  `STAT.md` page, but explicit in both this ROM's own header comment
  and Gekkio's mooneye-gb (`hardware/ppu.rs`'s `switch_mode()` VBlank
  arm, which does two independent, unconditional interrupt requests -
  one for Mode 1 if selected, one for Mode 2 if selected). Modeled as
  a direct, unconditional check alongside (not through) the general
  edge-triggered line, since it's a genuine glitch independent of the
  shared line's normal mode-based tracking.

**The remaining 7** (`hblank_ly_scx_timing-GS.gb`, 4 `intr_2_*.gb`
ROMs, `lcdon_timing-GS.gb`, `lcdon_write_timing-GS.gb`) all measure
exact-cycle timing relative to mode transitions via NOP-padded loops -
`hblank_ly_scx_timing-GS.gb`'s own header states the expected result
outright: "SCX mod 8 = 0 => LY increments 51 cycles after STAT
interrupt; 1-4 => 50; 5-7 => 49." Traced to a real, confirmed gap via
mooneye-gb's own `ppu.rs`: its `emulate()` function requests Mode 0's
STAT interrupt **one T-state before** the actual Mode 3->0 switch
(`// STAT mode=0 interrupt happens one cycle before the actual mode
switch!`, its own comment) - something `ppu.h`'s existing design
explicitly never modeled (mode boundaries are only checked once per
whole `gb_ppu_step()` call, i.e. once per whole CPU instruction, not
per T-state). Getting this right needs the PPU ticked from literally
every T-state - the same category of architecture gap
`docs/GAMEBOY_ROADMAP.md`'s "Timer M-cycle precision: attempted,
reverted" entry already found and documented for the timer, not
attempted here for the same reason.

A genuine, unplanned bonus: `make gameboy-visual-test`'s dmg-acid2
match rate improved from 98.04% to **99.71%** (22589/23040 ->
22974/23040 pixels) as a direct consequence of STAT interrupts now
firing (and blocking) correctly during that ROM's own real
STAT-driven raster tricks - not a targeted fix.

Zero regressions across the full existing suite. All 5 fixed ROMs
added to `EXPECTED` as `PASS`, the 7 still-open ones as `FAIL` (a
real, currently-accurate baseline) - **73/83** on the committed
Mooneye subset, up from 68/71.

## Results: the per-M-cycle CPU rewrite - 74/83, one real regression accepted and documented

Every opcode handler in `cpu.c` now self-ticks DMA/timer/PPU/APU once
per real M-cycle it takes (`gb_mcycle_tick()`, `mmu.c`), replacing the
old two-tier model where only a curated dozen-plus "DMA-precise"
opcodes self-ticked and everything else got one lump-sum tick after
the whole instruction. This is the rewrite the earlier "Timer M-cycle
precision: attempted, reverted" investigation (`docs/GAMEBOY_ROADMAP.md`)
concluded was necessary but didn't attempt in full - attempted for
real this time.

- **`pop_timing.gb` (FIXED)**: the ROM that motivated the whole
  investigation - needed exactly this precision.
- **`acceptance/ppu/hblank_ly_scx_timing-GS.gb` (FIXED)**: confirms the
  PPU side of the same gap was real too.
- **`acceptance/timer/rapid_toggle.gb` (regressed, investigated at
  length, accepted as a known gap)**: fails with `BC` off by exactly
  one "spurious tick" iteration - the same symptom the original,
  much-earlier attempt hit. Hand-verified every opcode's tick count
  against its own T-state total (all correct) and instrumented the
  actual `sys_counter`/TIMA/`overflow_delay` trace through the failing
  run (the spurious-tick mechanism itself behaves exactly as designed)
  without finding the remaining T-state-level discrepancy. Notably the
  one Mooneye ROM in the whole committed suite whose own header
  documents real hardware disagreeing across revisions ("pass: DMG
  ABC, MGB, CGB, AGB, AGS; fail: DMG 0") - see
  `docs/GAMEBOY_ROADMAP.md`'s matching entry for the full
  investigation. Recorded honestly in `EXPECTED` as `FAIL` rather than
  reverting the two real fixes above to avoid it.
- `test_roms/2048-gb/reference_frame.ppm` recaptured (its own
  README.md) - a benign, reconfirmed-correct side effect (its
  tile-spawn RNG seeds from a DIV read this rewrite made more precise),
  not a bug.

Zero regressions anywhere else. **74/83** on the committed Mooneye
subset, up from 73/83.

## Results: STAT read/OAM access timing lag - 3 more `acceptance/ppu/` ROMs, 77/83

Picked back up the 6 `acceptance/ppu/` timing ROMs the per-M-cycle
rewrite left open. Root-caused with hand-verified T-state
instrumentation (mode-transition timestamps and NOP-padded
polling-loop iteration counts, cross-checked against a by-hand trace of
the same instruction sequence), not guessed at. 3/6 fixed.

- **`intr_2_mode0_timing.gb`/`intr_2_mode3_timing.gb` (FIXED)**: the
  real mechanism is that `gb_mcycle_tick()` (`mmu.c`) ticks the PPU and
  then immediately performs the CPU's own memory access for that same
  M-cycle - so a STAT register read landing on the *exact* M-cycle a
  mode transition occurs sees the new mode immediately, where real
  hardware only makes it externally visible from the *next* M-cycle
  on. First tried a naive fix (delaying every mode-transition check
  uniformly via `dots > threshold` instead of `>=`) - this also delayed
  the Mode 0->2 boundary the ROM's own HALT-based synchronization
  relies on, so the *relative* timing between sync point and measured
  event never changed and it still failed. The real fix needed to be
  asymmetric: a new `ppu->visible_mode` (`ppu.h`), snapshotted one
  M-cycle behind `mode`, used only by `gb_ppu_read_reg()`'s STAT case -
  while interrupt-triggering logic keeps using `mode` directly,
  unlagged, since that's independently already correct
  (`stat_irq_blocking.gb`/`vblank_stat_intr-GS.gb`/`stat_lyc_onoff.gb`
  all still pass unchanged).
- **`intr_2_oam_ok_timing.gb` (FIXED)**: found investigating this
  cluster, a separate and more broadly consequential gap -
  `mmu.c` only ever blocked CPU access to OAM during active DMA;
  pandocs' `Rendering.md` "PPU modes" table documents OAM as
  inaccessible during Modes 2 *and* 3 too, independent of DMA. Added
  `gb_ppu_oam_blocked()` (using the same `visible_mode` lag) and wired
  it into `mmu.c`'s OAM read/write paths. Needed one companion fix:
  `ppu.c`'s own *internal* OAM reads (object selection, Mode 3 length,
  rendering) previously went through the same `gb_read_byte()` the new
  block now applies to, which would have blocked the PPU from reading
  its own OAM during the exact modes it needs to render sprites at
  all - fixed by routing those through a new `read_oam_internal()`
  that reads `cpu->memory[]` directly, the same "the PPU's own access
  is never blocked by logic that exists to block the CPU" pattern
  `gb_dma_tick()`'s own destination write already established.
- **Still open**: `intr_2_mode0_timing_sprites.gb` (an exhaustive 60+
  case stress test of `compute_mode3_length()`'s own OBJ-penalty
  formula specifically, untouched by the fix above) and
  `lcdon_timing-GS.gb`/`lcdon_write_timing-GS.gb` (both test a
  documented "PPU is late by 2 T-cycles" special case on the first
  line after LCD is enabled - no dedicated model for this exists yet).

Also added `stat_line` and `visible_mode` to `savestate.c`'s PPU
section (`SAVESTATE_VERSION` 2->3) - both are live PPU state a
save/load round trip previously dropped silently; `stat_line` was a
pre-existing gap from the earlier STAT-interrupt-model rework, found in
passing.

Zero regressions across the full existing suite. **77/83** on the
committed Mooneye subset, up from 74/83.

## Results: OBJ-penalty formula and Mode 3->0 rounding - `intr_2_mode0_timing_sprites.gb` fixed, 78/83

Picked up the largest of the 3 `acceptance/ppu/` ROMs the pass above
left open: an exhaustive 105-case stress test of
`compute_mode3_length()`'s OBJ-penalty formula (1-10 OBJs, X positions
across the full 0-255 range including off both screen edges, several
two-group split configurations) and how that value feeds the Mode 3->0
transition.

Its full `.s` source was fetched (`gh api repos/Gekkio/
mooneye-test-suite/contents/acceptance/ppu/
intr_2_mode0_timing_sprites.s`) and hand-decoded into every testcase's
OBJ configuration and expected extra-cycle count. A from-scratch Python
reimplementation of `compute_mode3_length()`, run against all 105
testcases, found three real, distinct bugs:

- **X==0 OBJs skipped tile-dedup entirely (FIXED)**: the exception
  ("OAM X==0 always costs 11 dots flat") was implemented as an early
  `continue`, so *every* X==0 OBJ added a full 11 dots regardless of
  whether an earlier OBJ (X==0 or not) already "considered" that same
  off-screen tile. Real hardware still runs X==0 OBJs through the
  normal dedup: `n` OBJs all at X==0 cost `11 + 6*(n-1)` dots, not
  `11*n` - confirmed by the ROM's own 2-through-10-OBJs-at-X==0
  testcases. Fixed by letting X==0 OBJs join the normal per-tile dedup
  loop, replacing only the *wait* component with a fixed 5 (not the
  unconditional flat 6-dot fetch) when the tile is new -  5+6=11
  matches the documented single-OBJ total exactly.
- **OBJs entirely off the right edge weren't skipped (FIXED)**: an OBJ
  with OAM X>=168 (leftmost screen column already >=160) was still
  charged a full penalty despite never being reached by the pixel
  fetcher this scanline - the ROM's own obj_x=168/169 testcases are
  the only ones in the suite asserting *zero* OBJ penalty despite
  objects being selected for the line (selection only checks Y).
  Fixed by skipping such OBJs entirely.
- **Mode 3->0 needs a rounded-down threshold when an OBJ was fetched
  (FIXED)**: the real rule isn't the same `ceil(mode3_dots/4)` plain
  `>=` check an OBJ-free scanline uses - it's 1 M-cycle earlier,
  against `mode3_dots` rounded *down* to the nearest M-cycle
  (`mode3_dots & ~3`), still with plain `>=`. `ppu->dots` keeps
  carrying the *unrounded* `mode3_dots` into Mode 0 afterward so the
  456-dot scanline budget is unaffected. Two wrong hypotheses (a flat
  "-1 M-cycle always" rule, and a naive "`>` instead of `>=`" - the
  wrong *direction*, making an exact-multiple `mode3_dots` transition
  *later* not earlier) both regressed the same 4 already-passing
  non-sprite `acceptance/ppu/` ROMs before this was found; needed
  direct T-state-level tracing of a real dispatch-to-poll instruction
  sequence to disambiguate from an offline model alone. New
  `ppu->mode3_had_obj` field (`ppu.h`/`ppu.c`, `savestate.c`,
  `SAVESTATE_VERSION` 3->4) gates the rounding to exactly the
  scanlines that need it.

Zero regressions across the full existing suite. **78/83** on the
committed Mooneye subset, up from 77/83.
