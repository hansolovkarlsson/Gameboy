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

## Results: 24/44 on first run, 28/44 now

Not 20 unrelated mysteries - grounded by reading each failing test's
real `.s` source (`gh api repos/Gekkio/mooneye-test-suite/contents/...`)
rather than guessed at, they trace to five real root causes. Four have
since been fixed in a follow-up pass; see `docs/GAMEBOY_ROADMAP.md`'s
own Mooneye status entries for the full story of both passes:

- **14 ROMs, still open** (`call_timing`, `call_cc_timing`/
  `call_cc_timing2`, `jp_timing`, `jp_cc_timing`, `push_timing`,
  `pop_timing`, `ret_timing`, `ret_cc_timing`, `reti_timing`,
  `rst_timing`, `ld_hl_sp_e_timing`, `add_sp_e_timing`) all use the
  identical technique: start a real OAM DMA transfer, pad with `nop`s
  tuned so a specific M-cycle of the instruction under test lands
  exactly inside vs. just after the DMA window, then check whether that
  access saw DMA-source garbage. This is a direct, precise hit on the
  exact gap `ppu.c` already documents from Phase 3: OAM DMA is an
  instant copy here, not a real timed 160-M-cycle transfer - correct
  for the universal busy-wait-in-HRAM convention real code uses, wrong
  for exactly this kind of adversarial mid-transfer access these tests
  are built to probe. One known, already-documented cause, not
  fourteen - but closing it for real needs interleaved per-M-cycle
  stepping around every memory access in `cpu.c`'s opcode handlers, a
  genuine architecture change, not attempted here.
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
  of 8 before). The remaining 1 assertion each still fails; plausibly
  the same instruction-granular (not real per-M-cycle) limitation as
  the OAM DMA cluster above, not yet root-caused further.

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
- **`mbc1/multicart_rom_8Mb.gb` (not attempted)**: a genuinely distinct
  MBC1 hardware variant, not a regular large-ROM MBC1 cart bug. MBC1M
  multi-game compilation carts wire bit 4 of the `$2000-3FFF` ROM bank
  register out entirely (pandocs' `MBC1.md` "MBC1M addressing
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
