# Mooneye GB Test Suite (Tier 1)

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
Committed subset ("Tier 1" - see the scoping discussion this came
from): `acceptance/timer/` (13), `acceptance/bits/` (3),
`acceptance/interrupts/ie_push.gb`, `acceptance/instr/daa.gb`, and 26
top-level CPU/interrupt-timing ROMs - 44 total.

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
- `emulator-only/mbc1/`, `emulator-only/mbc5/` (21 ROMs) - real value
  (an independent, non-synthetic reference check on `cart.c`'s banking
  beyond `tests/test_cart.c`'s own struct-level tests), left for a
  follow-up slice rather than this one.
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

## Real first-run results: 24/44 pass

Not 20 unrelated mysteries - grounded by reading each failing test's
real `.s` source (`gh api repos/Gekkio/mooneye-test-suite/contents/...`)
rather than guessed at:

- **14 ROMs** (`call_timing`, `call_cc_timing`/`call_cc_timing2`,
  `jp_timing`, `jp_cc_timing`, `push_timing`, `pop_timing`,
  `ret_timing`, `ret_cc_timing`, `reti_timing`, `rst_timing`,
  `ld_hl_sp_e_timing`, `add_sp_e_timing`) all use the identical
  technique: start a real OAM DMA transfer, pad with `nop`s tuned so a
  specific M-cycle of the instruction under test lands exactly inside
  vs. just after the DMA window, then check whether that access saw
  DMA-source garbage. This is a direct, precise hit on the exact gap
  `ppu.c` already documents from Phase 3: OAM DMA is an instant copy
  here, not a real timed 160-M-cycle transfer - correct for the
  universal busy-wait-in-HRAM convention real code uses, wrong for
  exactly this kind of adversarial mid-transfer access these tests are
  built to probe. One known, already-documented cause, not fourteen.
- **`if_ie_registers`, `interrupts/ie_push`**: a real, separate,
  narrower gap - cycle-exact behavior when `IE` is written *during*
  interrupt dispatch's PC push (can cancel/redirect the dispatch
  mid-flight). Not implemented.
- **`bits/unused_hwio-GS`**: unused/unmapped `$FFxx` bits should read
  back forced to `1`; several registers here don't.
- **`timer/tima_write_reloading`, `timer/tma_write_reloading`**: a
  finer-grained, single-T-state-precision sub-case of the
  TIMA-overflow-reload quirk Phase 4 already partially modeled (see
  `timer.c`).
- **`rapid_di_ei`**: a real, separate EI-delay edge case - rapid DI/EI
  toggling with no real instruction between them must never actually
  enable interrupts. Different ground than `ei_sequence`/`ei_timing`
  (both pass already); this specific case isn't covered by those.

None of these were fixed as part of adopting this suite - the point of
this pass was landing a real, independent, committable correctness
signal (closing the gap Phase 1 originally flagged Mooneye for: Blargg
ROMs can't be committed at all), not fixing every gap it immediately
found. See `tests/run_mooneye.py`'s own `EXPECTED` table for the
per-ROM baseline this locks in as a regression floor.
