#!/usr/bin/env python3
"""Run every Mooneye GB Test Suite ROM committed under test_roms/mooneye/
against bin/gameboy and check its real pass/fail result.

Mooneye (<https://github.com/Gekkio/mooneye-test-suite>, MIT-licensed -
see test_roms/mooneye/README.md for the fetch/commit story and why only
a curated subset is committed here) reports its result over the same
SB/SC internal-clock serial transfer Blargg's cpu_instrs/dmg_sound
already use (src/mmu.c's gb_serial_output_hook), not printable text: a
pass writes the Fibonacci sequence 3/5/8/13/21/34 to B/C/D/E/H/L and
sends those six bytes over serial; a failure sends the byte 0x42 six
times instead (see the suite's own README's "Pass/fail reporting"
section). Every ROM then loops on itself forever, so there's no clean
"done" signal beyond that - main.c's own fixed 20,000,000-instruction
default budget is what actually bounds each run.

Real first-run results (see test_roms/mooneye/README.md for the full
story, including what's since been fixed): 24/44 pass initially. The 20
that didn't weren't 20 unrelated mysteries - grounded by reading each
failing test's real .s source rather than guessed at, and traced to
five root causes, four fixed in a first follow-up pass (28/44):

- if_ie_registers/ie_push (FIXED): cycle-exact behavior when IE is
  written *during* interrupt dispatch's PC push - real hardware decides
  the vector fresh right after the high-byte push, not before either
  push or after both; see gb_cpu_step()'s interrupt-dispatch comment.
- bits/unused_hwio-GS (FIXED): unused/unmapped $FFxx bits (SC bits 1-6,
  IF bits 5-7, STAT bit 7, and several fully-unmapped registers) now
  read back forced to 1 - see mmu.c/ppu.c.
- rapid_di_ei (FIXED): a DI immediately following EI now genuinely
  cancels EI's still-pending delayed enable instead of a stale
  end-of-step re-apply silently overriding DI's own effect - see
  gb_op_di()/di_cancels_ei_delay in cpu.c/cpu.h.
- timer/tima_write_reloading, tma_write_reloading (PARTIALLY FIXED):
  the general cycle-A/cycle-B TIMA-overflow-reload write rule from
  pandocs' Timer_Obscure_Behaviour.md is now implemented and covered by
  a direct unit test (tests/test_timer.c) - 6 of these two ROMs'
  combined 8 assertions now pass (up from 0), confirmed by rendering
  each ROM's own on-screen diagnostic. The remaining 1 assertion each
  still fails - see below, since it turned out to be the same root
  cause as the *_timing cluster.
- 14 *_timing ROMs, all using start_oam_dma + tuned nop padding to land
  a specific M-cycle inside vs. just after the DMA window: a direct,
  precise hit on the exact gap ppu.c used to document from Phase 3 -
  OAM DMA was an instant copy, not a real timed 160-M-cycle transfer.
  13 of these 14 (add_sp_e_timing, call_timing/call_timing2/
  call_cc_timing/call_cc_timing2, jp_timing/jp_cc_timing,
  ld_hl_sp_e_timing, push_timing, ret_timing/ret_cc_timing/reti_timing,
  rst_timing) are now FIXED by a real, timed per-M-cycle OAM DMA
  rewrite (GBCpu.dma_active/dma_progress/gb_dma_tick() in cpu.h/mmu.c,
  driven by explicit per-M-cycle ticks in cpu.c's own CALL/JP/RET/RST/
  PUSH/POP/ADD SP,e8/LD HL,SP+e8 handlers - see cpu.c's own
  is_dma_precise_op() comment) - see test_roms/mooneye/README.md for
  the full story, including the real-hardware model this was cross-
  checked against (Gekkio's own mooneye-gb reference emulator) and the
  one subtle bug that took two passes to find (LDH (a8),A - the exact
  instruction Mooneye's own start_oam_dma macro uses - needed the same
  M-cycle-precise ticking as the 12 "obviously timing-sensitive"
  opcodes, or the whole transfer's timing base point would land 2
  M-cycles too early).
- pop_timing.gb (14th ROM, mis-clustered with the other 13 above in the
  original diagnosis - fully read now rather than skimmed): NOT an
  OAM DMA test at all. It points SP directly at the DIV register
  ($FF04) and checks whether POP's own reads see DIV's own mid-
  instruction increment at specific M-cycle boundaries - a real,
  distinct gap (this emulator only advances the timer once per whole
  instruction, in a lump sum, not per M-cycle) that happens to need the
  same *kind* of fix (per-M-cycle precision) but for the timer, not
  DMA. Left open, honestly re-scoped rather than folded into "still
  needs the OAM DMA architecture change" the way it was before this
  pass - that gap is now closed, this one wasn't in scope for it.
  (This is also why tima_write_reloading/tma_write_reloading's last
  unresolved assertion, above, never got any closer: same underlying
  cause.)

A follow-up slice added Tier 2's emulator-only/mbc1 and
emulator-only/mbc5 (21 ROMs, see test_roms/mooneye/README.md): 20/21
pass.

- All 8 mbc5/rom_*.gb ROMs (FIXED): a real bug, not a test-harness
  quirk - MBC5 has no read-time "bank 0 reads as bank 1" quirk at
  $4000-7FFF (unlike MBC1/MBC3), so cart.c's gb_cart_load() left a
  fresh MBC5 cart's ROM bank register at its calloc-zeroed 0, showing
  bank 0's content at $4000-7FFF instead of bank 1's. Every one of
  these 8 ROMs calls straight into ROMX-bank library code before ever
  writing the bank register, so all 8 failed identically (crashed into
  the $0038 RST trap almost immediately - confirmed by tracing PC).
  Real MBC5 hardware powers up with that register at 1, not 0 -
  verified against Gekkio's own mooneye-gb (cartridge.rs,
  Mbc5State::default()), a real-hardware-checked reference. Fixed in
  gb_cart_load() (cart.c); also covered by a direct unit test
  (tests/test_cart.c's test_mbc5_default_bank_is_one()).
- mbc1/multicart_rom_8Mb.gb (FIXED, follow-up slice): MBC1M multi-game
  compilation carts wire the same two MBC1 registers differently -
  pandocs' MBC1.md "MBC1M" section: the secondary 2-bit register lands
  on bank-number bits 4-5 instead of 5-6, and the primary 5-bit
  register is truncated to its low 4 bits for banking (the full 5 bits
  still feed the existing 0->1 quirk first, computed before any
  multicart truncation). Detected at load time via
  is_mbc1_multicart(), ported directly from Gekkio's mooneye-gb
  (core/src/config/cartridge.rs) since - as this ROM's own .s source
  says - "MBC1 multicarts *cannot* be detected from the header alone":
  only a real 1 MiB ROM with a valid Nintendo logo at 3 of its 4
  256 KiB page boundaries (mooneye-gb's own >=3-of-4 threshold, not all
  4, to tolerate a menu-less layout) gets flagged, so no regular 1 MiB
  MBC1 game (logo in page 0 only) misfires as a multicart. See cart.c's
  own is_mbc1_multicart()/gb_cart_read() comments for the full
  citations and formula.

A later follow-up slice added Tier 2's acceptance/oam_dma* (6 ROMs,
never fetched before now that OAM DMA is real and timed): 6/6 pass,
after two more real gaps in the OAM-DMA-timing rewrite itself.

- oam_dma_start.gb/oam_dma_timing.gb/oam_dma_restart.gb (FIXED): all
  three needed a real memory access - LD (HL),A (self-modified into
  ROM to trigger the $FF46 write, oam_dma_start.gb's own trick) or LD
  A,(HL) (reading OAM back at a NOP-padded instant meant to land
  exactly one T-state before vs. after DMA's last copy,
  oam_dma_timing.gb) - to be M-cycle-precise against DMA, same root
  cause as the earlier LDH (a8),A gap the 13/14 OAM-DMA-timing fix
  found: gb_op_ld_r_r (cpu.c, covers the whole LD r,r'/LD r,(HL)/LD
  (HL),r 0x40-0x7F block) was a lump-sum "fallback" opcode, so its one
  real memory access could land up to 1 M-cycle off from where DMA's
  own state said it should. Fixed by ticking once before that access,
  the same pattern as LDH (a8),A; added to is_dma_precise_op()'s set.
- oam_dma/sources-GS.gb (FIXED): a genuine, separate hardware quirk,
  not a timing gap - DMA's own address generator has no OAM/I-O special
  case the way the CPU's normal bus decoder does, so a source page of
  $E0-$FF (pandocs' OAM_DMA_Transfer.md only documents $00-$DF as
  valid) actually reads WRAM at $C000-$DFFF instead - real hardware's
  page with bit 5 (0x20) cleared. Confirmed against Gekkio's own
  mooneye-gb (hardware.rs's emulate_oam_dma(): source pages 0xe0..=0xef
  and 0xf0..=0xff route to the same work_ram.read_lower()/
  read_upper() calls as 0xc0..=0xcf/0xd0..=0xdf) and against this ROM's
  own assertions (sourcing DMA from page $FE/$FF and expecting OAM to
  match whatever was written to $DE00/$DF00 beforehand). Fixed in
  gb_dma_tick() (mmu.c).

EXPECTED below is this real, current baseline, the same floor-not-target
reasoning tests/compare_frame.py already uses for dmg-acid2: a ROM
regressing from PASS to anything else is a real regression and fails
this script; a currently-failing ROM starting to pass is real progress,
reported but not treated as a failure (update EXPECTED when that
happens, rather than the script silently hiding it).
"""
import subprocess
import sys
from pathlib import Path

PASS_SEQUENCE = bytes([3, 5, 8, 13, 21, 34])
FAIL_SEQUENCE = bytes([0x42] * 6)

# Real, current per-ROM baseline - see this file's own top comment for
# the grounded root-cause breakdown behind every "FAIL" entry below.
EXPECTED = {
    "acceptance/add_sp_e_timing.gb": "PASS",
    "acceptance/bits/mem_oam.gb": "PASS",
    "acceptance/bits/reg_f.gb": "PASS",
    "acceptance/bits/unused_hwio-GS.gb": "PASS",
    "acceptance/call_cc_timing.gb": "PASS",
    "acceptance/call_cc_timing2.gb": "PASS",
    "acceptance/call_timing.gb": "PASS",
    "acceptance/call_timing2.gb": "PASS",
    "acceptance/di_timing-GS.gb": "PASS",
    "acceptance/div_timing.gb": "PASS",
    "acceptance/ei_sequence.gb": "PASS",
    "acceptance/ei_timing.gb": "PASS",
    "acceptance/halt_ime0_ei.gb": "PASS",
    "acceptance/halt_ime0_nointr_timing.gb": "PASS",
    "acceptance/halt_ime1_timing.gb": "PASS",
    "acceptance/halt_ime1_timing2-GS.gb": "PASS",
    "acceptance/if_ie_registers.gb": "PASS",
    "acceptance/instr/daa.gb": "PASS",
    "acceptance/interrupts/ie_push.gb": "PASS",
    "acceptance/intr_timing.gb": "PASS",
    "acceptance/jp_cc_timing.gb": "PASS",
    "acceptance/jp_timing.gb": "PASS",
    "acceptance/ld_hl_sp_e_timing.gb": "PASS",
    "acceptance/oam_dma/basic.gb": "PASS",
    "acceptance/oam_dma/reg_read.gb": "PASS",
    "acceptance/oam_dma/sources-GS.gb": "PASS",
    "acceptance/oam_dma_restart.gb": "PASS",
    "acceptance/oam_dma_start.gb": "PASS",
    "acceptance/oam_dma_timing.gb": "PASS",
    "acceptance/pop_timing.gb": "FAIL",
    "acceptance/push_timing.gb": "PASS",
    "acceptance/rapid_di_ei.gb": "PASS",
    "acceptance/ret_cc_timing.gb": "PASS",
    "acceptance/ret_timing.gb": "PASS",
    "acceptance/reti_intr_timing.gb": "PASS",
    "acceptance/reti_timing.gb": "PASS",
    "acceptance/rst_timing.gb": "PASS",
    "acceptance/timer/div_write.gb": "PASS",
    "acceptance/timer/rapid_toggle.gb": "PASS",
    "acceptance/timer/tim00.gb": "PASS",
    "acceptance/timer/tim00_div_trigger.gb": "PASS",
    "acceptance/timer/tim01.gb": "PASS",
    "acceptance/timer/tim01_div_trigger.gb": "PASS",
    "acceptance/timer/tim10.gb": "PASS",
    "acceptance/timer/tim10_div_trigger.gb": "PASS",
    "acceptance/timer/tim11.gb": "PASS",
    "acceptance/timer/tim11_div_trigger.gb": "PASS",
    "acceptance/timer/tima_reload.gb": "PASS",
    "acceptance/timer/tima_write_reloading.gb": "FAIL",
    "acceptance/timer/tma_write_reloading.gb": "FAIL",
    "emulator-only/mbc1/bits_bank1.gb": "PASS",
    "emulator-only/mbc1/bits_bank2.gb": "PASS",
    "emulator-only/mbc1/bits_mode.gb": "PASS",
    "emulator-only/mbc1/bits_ramg.gb": "PASS",
    "emulator-only/mbc1/multicart_rom_8Mb.gb": "PASS",
    "emulator-only/mbc1/ram_256kb.gb": "PASS",
    "emulator-only/mbc1/ram_64kb.gb": "PASS",
    "emulator-only/mbc1/rom_16Mb.gb": "PASS",
    "emulator-only/mbc1/rom_1Mb.gb": "PASS",
    "emulator-only/mbc1/rom_2Mb.gb": "PASS",
    "emulator-only/mbc1/rom_4Mb.gb": "PASS",
    "emulator-only/mbc1/rom_512kb.gb": "PASS",
    "emulator-only/mbc1/rom_8Mb.gb": "PASS",
    "emulator-only/mbc5/rom_16Mb.gb": "PASS",
    "emulator-only/mbc5/rom_1Mb.gb": "PASS",
    "emulator-only/mbc5/rom_2Mb.gb": "PASS",
    "emulator-only/mbc5/rom_32Mb.gb": "PASS",
    "emulator-only/mbc5/rom_4Mb.gb": "PASS",
    "emulator-only/mbc5/rom_512kb.gb": "PASS",
    "emulator-only/mbc5/rom_64Mb.gb": "PASS",
    "emulator-only/mbc5/rom_8Mb.gb": "PASS",
}


def run_one(gameboy_bin, rom_path):
    try:
        result = subprocess.run(
            [gameboy_bin, str(rom_path)],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            timeout=30,
        )
    except subprocess.TimeoutExpired:
        return "TIMEOUT"
    output = result.stdout
    if PASS_SEQUENCE in output:
        return "PASS"
    if FAIL_SEQUENCE in output:
        return "FAIL"
    return "UNKNOWN"


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <bin/gameboy> <test_roms/mooneye dir>", file=sys.stderr)
        return 1
    gameboy_bin = sys.argv[1]
    root = Path(sys.argv[2])
    roms = sorted(root.rglob("*.gb"))
    if not roms:
        print(f"No .gb ROMs found under {root}", file=sys.stderr)
        return 1

    results = {}
    for rom in roms:
        name = str(rom.relative_to(root))
        status = run_one(gameboy_bin, rom)
        results[name] = status
        print(f"  {status:8s} {name}")

    passed = [n for n, s in results.items() if s == "PASS"]
    print(f"\n{len(passed)}/{len(results)} passed")

    regressions = []
    progress = []
    unknown_roms = []
    for name, status in sorted(results.items()):
        expected = EXPECTED.get(name)
        if expected is None:
            unknown_roms.append(name)
        elif expected == "PASS" and status != "PASS":
            regressions.append((name, status))
        elif expected == "FAIL" and status == "PASS":
            progress.append(name)

    if progress:
        print("\nReal progress since the committed baseline (update EXPECTED in this script):")
        for name in progress:
            print(f"  now PASS: {name}")

    if unknown_roms:
        print("\nROMs with no baseline entry in EXPECTED (add one):")
        for name in unknown_roms:
            print(f"  {name}")

    if regressions:
        print("\nRegressions against the committed baseline:")
        for name, status in regressions:
            print(f"  expected PASS, got {status}: {name}")
        return 1
    if unknown_roms:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
