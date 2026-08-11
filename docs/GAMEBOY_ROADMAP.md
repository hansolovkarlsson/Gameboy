# Game Boy Emulator Roadmap

## Project

A Game Boy (DMG, the original 1989 hardware) emulator. Originally
developed as a separate subproject inside a Z80/CP-M emulator repo
(sharing nothing but a build directory and general-purpose tooling with
it - see the "Architecture decision" section just below), then split
out into this standalone repo via `git subtree split` once real,
end-to-end functionality (a real front end, real-game validation, save
states) made clear the two projects would never actually share code -
see `README.md` for the directory layout and why cartridge ROMs are
split into `roms/` (your own dumps, never committed) vs `test_roms/`
(open-source test suites, safe to commit). This document tracks
*status* (see below - this project is far along, not a from-scratch
scoping document any more) - for the actual SM83 instruction set and
DMG hardware behavior (memory map, PPU, APU, timer, joypad), see
`docs/CPU_REFERENCE.md` and `docs/HARDWARE_REFERENCE.md` instead, the
same docs/status split the sibling Z80/CP-M repo uses for itself
(`cpm/docs/ROADMAP.md`/`cpm/docs/Z80_REFERENCE.md`/
`cpm/docs/CPM_REFERENCE.md`), not invented fresh here.

## The CPU: Sharp SM83 (commonly called "LR35902")

Z80-*derived*, not a Z80. The real, confirmed differences from the
Z80 core already in `cpm/emu/src/z80.c`:

- No `IX`/`IY` index registers, and therefore none of the `DD`/`FD`-
  prefixed instructions or `(IX+d)`/`(IY+d)` addressing.
- No alternate register set - no `EXX`, no `EX AF,AF'`.
- No `ED`-prefixed block instructions (`LDIR`, `CPIR`, etc.) - the
  `ED` prefix space is mostly unused on this CPU.
- Adds its own instructions the Z80 doesn't have, most notably `STOP`
  and `LD (HL+),A`/`LD (HL-),A`/`LD A,(HL+)`/`LD A,(HL-)` (auto-
  increment/decrement variants used constantly by real Game Boy code).
- No `IN`/`OUT` - I/O is entirely memory-mapped (`0xFF00`-`0xFF7F`).
- `DAA`'s behavior differs from the Z80's in ways worth verifying
  against a real reference rather than assuming Z80 semantics carry
  over unchanged.
- Runs at ~4.194304 MHz; the Z80 core's T-state cycle-counting
  approach still applies, but real timing values need to come from a
  grounded Game Boy reference, not carried over from Z80 timings.

**Primary references** (the same "ground everything in a real primary
source, don't guess" discipline `CLAUDE.md` already documents for the
Z80/CP-M side): [Pan Docs](https://gbdev.io/pandocs/) is the
community-maintained definitive Game Boy hardware reference; the
[gbdev.io](https://gbdev.io/) "Awesome Game Boy Development" list
rounds up the rest (opcode tables, timing docs, test ROM sources).
Cite the specific page/section when a behavior is implemented from
one of these, the same way `cpm/resources/ccp/upstream/ccp.asm` gets
cited for real CCP behavior rather than "CP/M documentation summaries"
per `CLAUDE.md`'s own stated preference.

## Architecture decision: standalone core, not shared with `z80.c`

A previous session's notes (`cpm/docs/ROADMAP.md`'s old Phase 4 entry)
sketched extracting `z80.c`/`alu.c` into a shared `core/` and
parameterizing the opcode table for the SM83's differences. Starting
this project standalone instead: `src/` gets its own CPU core,
own opcode table, own ALU code, no dependency on `cpm/emu/src/`. Reasoning:
the ISA differences above are real and pervasive enough (missing
register files, a materially different instruction set, different
addressing modes) that a single parameterized dispatch table would
mean conditional logic threaded through nearly every opcode handler,
before a single Game Boy instruction has even been written and tested.
That's the premature abstraction `CLAUDE.md` already tells this project
to avoid ("Don't design for hypothetical future requirements"). If
real, painful duplication shows up once Phase 1 is underway - not
before - that's the time to extract something shared, informed by
what's actually common rather than what's guessed to be common now.

`cpm.c` (BDOS/BIOS emulation) doesn't apply to the Game Boy at all and
was never a candidate for sharing - that boundary already existed.

## Phases

### Phase 1: CPU core

The SM83 instruction set, table-driven dispatch in the same spirit as
`z80.c`'s `main_opcode_table`/`z80_op_prefix_cb` pattern (same *shape*
of solution, independently implemented - see the architecture decision
above). Gate: passing Blargg's `cpu_instrs.gb` and `instr_timing.gb`
test ROMs cleanly - the direct equivalent of ZEXALL/ZEXDOC gating the
Z80 core's Phase 1. Until this passes, nothing downstream (PPU, real
games) can be trusted to be running correct code.

### Phase 2: Memory map and cartridge/MBC support

The Game Boy's memory map (ROM banks, VRAM, external/cartridge RAM,
WRAM, OAM, I/O registers, HRAM) and at least enough cartridge-type
(MBC) support to load real ROMs: MBC-less (32KB ROM-only) first, then
MBC1, MBC3 (with its real-time-clock registers), MBC5. Needed before
any real game can even be loaded, let alone run.

### Phase 3: PPU (the LCD controller)

Background, window, and sprite (OAM) rendering, plus the PPU mode
timing (`OAM scan`/`Draw`/`H-Blank`/`V-Blank`) real games and test
ROMs depend on. This is the feature that makes the output "a Game Boy
screen" rather than a headless CPU exerciser - the natural next
correctness gate here is a well-known visual test ROM (e.g. from the
Mooneye suite) compared pixel-for-pixel against a known-good reference
image, not just "look at it and eyeball it."

### Phase 4: Interrupts, timer, joypad input

The interrupt controller (`IE`/`IF`, the five real interrupt sources -
V-Blank, LCD STAT, Timer, Serial, Joypad), the `DIV`/`TIMA`/`TMA`/`TAC`
timer registers, and joypad input (`0xFF00`). Needed for essentially
any real game to be playable at all, not just "boots to a logo."

### Phase 5: APU (sound)

The four sound channels (two pulse, one wave, one noise) and the
master sound registers. Deferred until video output actually works -
a silent screen is a much smaller loss than a black one, and audio
correctness is much harder to verify without ears on it during
development.

### Phase 6: Real-game validation

Same convention this project already uses on the CP/M side
(`CLAUDE.md`'s stated session convention: find a bug via real software,
root-cause it against a grounded reference, fix, add a regression
test): run real, well-known homebrew/open-source ROMs from
`test_roms/`, and - separately, locally, never committed -
real cartridge dumps from `roms/` the user owns, fixing bugs
found against Pan Docs/hardware test results rather than guessing.

### Phase 7: A real graphical front end, save states

Save states / battery-backed cartridge RAM persistence, and a real
graphical front end (the Game Boy's output is a pixel framebuffer, not
text, so the `cpm/gtk/` subproject's spawn-a-pty-and-let-`VteTerminal`
interpret-it approach doesn't transfer directly). Both done - see the
Status section's own Phase 7 entry.

### Phase 9: Game Boy Color (CGB) support

Cartridge CGB-flag detection, WRAM/VRAM banking, CGB tile attributes,
color palettes, double-speed mode (`KEY1`), HDMA/GDMA VRAM DMA
transfers, and the infrared port at register level (`RP`) - real CGB
color rendering plus the CPU speed switch, mid-frame VRAM streaming, and
IR register fidelity (real peer-to-peer IR communication is a separate,
deliberately out-of-scope networking feature). See the Status section's
own Phase 9 entries for the full scope and reasoning.

### Phase 10: original homebrew game content ("Prism")

The project's other stated long-term goal alongside CGB support, now
that the emulator core is mature enough to be a genuinely fun target -
`prism/`, a CGB-only grid-based color-matching puzzle written in
GBDK-2020 (C) rather than RGBDS. See the Status section's own Phase 10
entry for the concept, milestone roadmap, and toolchain reasoning.

### Phase 11: a second original homebrew game ("Wayfarer")

A second, separate homebrew game alongside `prism/` - `wayfarer/`, a
CGB-only top-down action-adventure in the tradition of the original
Zelda (1986), also written in GBDK-2020 (C), reusing the same toolchain
install `prism/` already needs. See the Status section's own Phase 11
entry for the concept, milestone roadmap, and design.

## Status

**Phase 1 (CPU core): functionally complete and passing its gate.**
`src/cpu.c`/`alu.c` implement the full SM83 instruction set -
every opcode in both the unprefixed and CB-prefixed tables, table-driven
in the same spirit as `cpm/emu/src/z80.c` (generic decode for the four fully
regular blocks: `LD r,r`, the r8 and d8 ALU groups, and the whole
CB-prefixed table; individually-named handlers for everything else).
`src/mmu.c` is a deliberately temporary flat-memory harness
(echo RAM, a "not usable" stub, a serial-port capture hook) - just
enough to run a real, unbanked ROM, not a real MMU (Phase 2's job).
`make gameboy` builds `bin/gameboy`, opt-in like `make gtk` (not part of
`all`/`test` yet - see the Makefile comment for why).

Every opcode's bytes/cycles/flags were checked against the official
gbdev.io opcode table
(<https://gbdev.io/gb-opcodes/optables/>, data at
`https://gbdev.io/gb-opcodes/Opcodes.json`) rather than trusted from
memory - this caught one real erratum in passing: a commonly-mirrored
community JSON dataset (`lmmendes/game-boy-opcodes`) lists `BIT b,(HL)`
as 16 cycles; the official table (and this emulator) has it at 12, since
`BIT` never writes anything back the way the other CB read-modify-write
ops do. DAA, the accumulator-vs-CB-prefixed rotate distinction, and
`ADD SP,e8`/`LD HL,SP+e8`'s flag quirks were all grounded the same way.
The HALT bug (pandocs' dedicated `halt.md` page, fetched during this
phase) is documented and has a field reserved for it in `GBCpu`, but
isn't implemented yet - it needs a real `IE`/`IF` (Phase 4) to detect
the "interrupt already pending" condition that triggers it.

**Correctness gate**: Blargg's `cpu_instrs` individual sub-tests
(fetched locally for testing - see the licensing note below) pass
10 of 11: `01-special`, `03-op sp,hl`, `04-op r,imm`, `05-op rp`,
`06-ld r,r`, `07-jr,jp,call,ret,rst`, `08-misc instrs`, `09-op r,r`,
`10-bit ops`, `11-op a,(hl)` all print `Passed`. The 11th,
`02-interrupts`, and the separate `instr_timing.gb` (which needs the
`DIV` register incrementing to measure elapsed cycles) both fail in
exactly the way expected: both need a real timer/interrupt controller,
which doesn't exist until Phase 4 - not a CPU-core correctness bug.

**Correction, made while scoping Mooneye adoption much later (see this
doc's own Status section, "Mooneye GB Test Suite adoption")**: the
paragraph below originally claimed Mooneye "ships as assembly source
needing `rgbds` to build." Checked against the real upstream
`Makefile` while actually scoping that work, and that's wrong - it
builds with WLA-DX (`wla-gb`/`wlalink`), a completely different
assembler, never RGBDS. Left the original (wrong) reasoning below
as-written rather than editing history, since it accurately reflects
what was believed and decided upon *at the time* Phase 1's own
toolchain call was made - see the Mooneye status entry for what
actually unblocked this.

**Licensing note - why `test_roms/` is still empty**: Blargg's
test ROMs (fetched from `retrio/gb-test-roms` to validate the above,
not committed) carry no explicit license, unlike ZEXALL/ZEXDOC (GPLv2,
committed at the sibling Z80/CP-M repo's own `cpm/emu/zexall/`) - the
same cautious call already documented in `README.md`. This also means
Blargg's ROMs can't be wired into a committed, automatically-run
regression target the way that sibling repo's own `make test` wires in
ZEXALL, since that would need either committing them anyway or a
network fetch at test time (neither matches this project's
reproducible-test convention). The Mooneye GB test suite
(`Gekkio/mooneye-test-suite`, confirmed MIT-licensed) is the recommended
path to a real, committable correctness gate of that same shape -
deferred rather than pursued now since it ships as assembly source
needing `rgbds` to build, not prebuilt ROMs, which is real additional
setup work of its own.

**Phase 2 (memory map and cartridge/MBC support): done.**
`src/cart.{c,h}` parses a real cartridge header (`0x0134`-
`0x014F` - title area, cartridge-type byte, ROM/RAM size codes, header
checksum) and implements MBC-less, MBC1, MBC3 (with its RTC latch
register set), and MBC5 bank switching - the scope this phase's own
plan set out, covering the overwhelming majority of real cartridges.
Every register layout and banking quirk (MBC1's "bank 0 reads as bank
1" translation and its simple-vs-advanced mode secondary register that
either extends ROM addressing *or* selects a RAM bank depending on
cartridge size; MBC3's RTC register-select-vs-RAM-bank overlap and its
0x00-then-0x01 latch sequence; MBC5's clean 9-bit ROM bank number with
no bank-0 translation quirk at all) is grounded against pandocs'
`MBC1.md`/`MBC3.md`/`MBC5.md`/`nombc.md`/`The_Cartridge_Header.md`
(fetched during this phase), not guessed. `src/mmu.c` now
routes `0x0000`-`0x7FFF` and `0xA000`-`0xBFFF` through `cart.c`;
`cpu->memory` (see `cpu.h`) is VRAM/WRAM/OAM/I-O-registers/HRAM only, no
longer the whole address space. `gb_cpu_reset()`'s F-register
simplification from Phase 1 (hardcoding the "nonzero header checksum"
case) is gone - it now reads the cartridge's real header-checksum byte,
per a pandocs footnote whose exact condition (is the checksum *byte*
zero, not whether it *validates* - a real Game Boy refuses to run a
cartridge whose checksum doesn't validate at all, so by the time code
is running the two almost always agree, but they're different
questions) was easy to get subtly wrong and worth citing precisely.

**Correctness gate**: none of Blargg's `cpu_instrs` ROMs use banking at
all (they're all plain 32 KiB MBC-less), so they don't exercise any of
this phase's actual work, and no real MBC1/MBC3/MBC5 test ROM was
available to fetch and commit (same licensing situation as `cpu_instrs`
- see above). Instead, `tests/test_cart.c` (`make gameboy-test`,
26 checks) unit-tests `cart.c` directly against the exact scenarios in
pandocs' own addressing diagrams - MBC1 basic and large-ROM/advanced-
mode banking, MBC1 RAM banking (both banking modes), RAM-disabled
behavior, MBC3 ROM banking and its RTC latch sequence, MBC5's 9-bit
banking, and `gb_cart_load()`'s real file-header-parsing path (the one
integration point `main.c` actually depends on, not exercised by the
struct-construction tests above it). This is this project's own code
testing its own code, so - unlike the ROM-based gates - it has no
licensing question and could be `make`-invoked directly; it's simply
not part of the top-level `all`/`test` yet, matching the rest of this
still-early subproject.

**Phase 3 (PPU): done, with a documented, evidenced gap.**
`src/ppu.{c,h}` implements the LCD controller: all twelve
registers (`0xFF40`-`0xFF4B`), the mode/timing state machine (OAM
scan/Drawing/HBlank/VBlank, 456 dots/scanline, 154 scanlines/frame),
and a scanline-at-a-time renderer covering background, window, and
objects (both 8x8 and 8x16, correct selection/drawing priority, X/Y
flip, the two tile-addressing modes and their signed-vs-unsigned
quirk, DMG palette translation). `src/mmu.c` now routes
`0xFF40`-`0xFF4B` through it and triggers OAM DMA transfers. Every
register layout, addressing mode, and priority rule is grounded
against pandocs' `LCDC.md`/`STAT.md`/`Tile_Data.md`/`Tile_Maps.md`/
`OAM.md`/`Rendering.md`/`Palettes.md`/`OAM_DMA_Transfer.md` (fetched
during this phase - see `ppu.c`'s own comments for which page backs
which rule), not guessed. Two deliberate simplifications, both
documented in `ppu.c` at the exact line they apply: Mode 3 is always
172 dots (the real minimum) rather than the real hardware's variable
172-289 (SCX/window/object timing penalties aren't modeled - affects
STAT-interrupt timing precision, not rendered pixel content); OAM DMA
is an instant 160-byte copy rather than the real timed 160 M-cycle
transfer (correct for any program that follows the universal
busy-wait-in-HRAM convention real hardware requires anyway).

**Correctness gate**: [dmg-acid2](https://github.com/mattcurrie/dmg-acid2)
(Matt Currie, MIT-licensed - committed at `test_roms/dmg-acid2/`,
unlike Blargg's ROMs) is the standard PPU correctness test in the Game
Boy dev community, with a known-correct reference image to compare
against pixel-for-pixel - exactly the gate this phase's own original
plan called for. `make gameboy-visual-test` renders a frame and runs
`tests/compare_frame.py` (a small dependency-free PNG decoder +
comparator, since there's no image library in this project) against it:
**21037/23040 pixels match (91.31%)**.

The remaining ~9% has a specific, evidenced cause, not a mystery: dmg-acid2's
own README states it "uses `LY`=`LYC` coincidence interrupts to perform
register writes on specific rows of the screen during mode 2" - nearly
every interesting visual feature (the window being toggled on/off for
the eyes and chin, `LCDC` bit 0 toggling to hide hair, the tile map
switching for the footer) is implemented as a mid-frame raster effect
driven by a STAT interrupt actually firing and being *handled*. This
project has interrupt *requests* (the PPU already sets `IF` bits on
VBlank/STAT events - see `ppu.c`) but not interrupt *dispatch* (jumping
to a handler when `IME`/`IE`/`IF` allow it), which is explicitly Phase
4, not built yet. A side-by-side comparison confirms this precisely:
the static parts (overall face shape, mouth, general background
structure - whatever was set up once before any interrupt would have
fired) render correctly, while every interrupt-gated detail is visibly
wrong or missing exactly as predicted - the footer text ("dmg-acid2 by
Matt Currie") is entirely blank (the window never gets disabled to
reveal it), the eyes render differently (their two-stage window/object
overlay never gets its mid-frame update), and the "HELLO WORLD!" text's
exclamation mark handling is affected (the row it's on is exactly where
`tests/compare_frame.py`'s pixel diff concentrates). Re-run this
gate once Phase 4 lands - a rate meaningfully *below* 91.31% at that
point would flag a real regression, which is why `compare_frame.py`
treats its baseline as a floor to check against, not a fixed target.

**Phase 4 (interrupts, timer, joypad input): done, dmg-acid2 prediction confirmed.**
`src/cpu.c`'s `gb_cpu_step()` now actually dispatches interrupts
(push `PC`, jump to `0x40`/`0x48`/`0x50`/`0x58`/`0x60`, 20 T-states,
priority by bit order) instead of just leaving `IF` bits set for no one
to read - grounded against pandocs' `Interrupts.md` (fetched during
this phase). The HALT bug (flagged unimplemented back in Phase 1) is
now real too: `IME=0` with an interrupt already pending at `HALT` time
sets `halt_bug` instead of actually halting, and `gb_cpu_step()`
replays the following instruction a second time with its real side
effects (not just a refetch) before continuing normally - matching
pandocs' `halt.md` precisely, including *why* it happens (a skipped PC
increment), not just the visible symptom.

`src/timer.{c,h}` (new) implements `DIV`/`TIMA`/`TMA`/`TAC`
(`0xFF04`-`0xFF07`) as the real hardware does: a free-running 16-bit
"system counter" (`DIV` is just its visible upper byte) with `TIMA`
incrementing on a *falling edge* of one specific counter bit (selected
by `TAC`'s clock-select field), not a naive independent periodic
counter. That choice isn't pedantry - it's what makes two genuinely
obscure, easy-to-get-wrong behaviors fall out for free instead of
needing special-casing: writing `DIV` (or executing `STOP`, which resets
the same counter) can cause a spurious `TIMA` tick if the monitored bit
happened to be set; and a `TIMA` overflow doesn't reload from `TMA` and
request an interrupt until one M-cycle *after* the overflow, reading
`$00` in between (pandocs' `Timer_Obscure_Behaviour.md`). Both are
covered by `tests/test_timer.c` (`make gameboy-test`, 14
checks) directly, independent of any ROM.

`src/joypad.{c,h}` (new) implements `P1`/`JOYP` (`0xFF00`) -
the action/direction button multiplexing and its inverted "0 = pressed"
polarity (pandocs' `Joypad_Input.md`) - and a `gb_joypad_set_action()`/
`gb_joypad_set_direction()` API for a future front-end or test harness
to call. No real input source exists yet (still Phase 7's job), so the
joypad reports "nothing pressed" for the whole run in `main.c` as of
this phase - the register and interrupt-request logic are real and
tested (correct multiplexing, correct polarity), just never actually
driven by anything yet.

**Correctness gate, part 1**: all 12 of Blargg's `cpu_instrs`/
`instr_timing` sub-tests now pass, including the two that failed back
in Phase 1/2 specifically *because* interrupts/timer didn't exist yet
(`02-interrupts`, `instr_timing`) - exactly the predicted outcome, not
a surprise.

**Correctness gate, part 2**: `make gameboy-visual-test` (dmg-acid2)
jumped from Phase 3's 91.31% to **22589/23040 (98.04%)** the moment
interrupt dispatch existed - direct, strong confirmation that the
Phase 3 diagnosis (nearly every visual detail is gated behind a
mid-frame `LY`=`LYC` interrupt actually firing and being handled) was
right, not a guess that happened to sound plausible. A side-by-side
comparison shows the predicted features now rendering correctly: the
"HELLO WORLD!" exclamation mark, correct eye rendering, and most of the
footer text ("dmg-acid2 by Ma..." - see below for what's still off).

**Remaining gap, honestly reported**: 451 pixels still mismatch,
concentrated in exactly two places - the top row (`LY=0`) of the
"HELLO WORLD!" banner (66 pixels, *unchanged* from Phase 3's count
before interrupt dispatch existed - direct proof this specific one
isn't interrupt-timing-related at all), and the tail end of the footer
text (`LY=133`-`141`, 385 pixels - the footer is *present* now, just
cut off partway, consistent with a timing-related issue this time).
Both are plausibly connected to the same root cause: `gb_ppu_step()`
still renders each scanline all at once when Mode 3 completes (Phase
3's documented simplification), rather than progressively pixel-by-
pixel the way real hardware's pixel FIFO does - dmg-acid2's own README
says its register writes happen "during mode 2 (OAM scan)" specifically
*because* real hardware can react within that window, but this
emulator's CPU/PPU/timer are stepped in sequence once per whole
instruction rather than interleaved sub-instruction, which can shift
exactly when an `LY`=`LYC` interrupt actually gets serviced relative to
when a scanline gets drawn. This is a real, open issue - not silently
swept under the "documented simplification" umbrella without stating
plainly that it isn't fully root-caused - see `compare_frame.py`'s own
95%-floor regression check (set from this 98.04% baseline, not 100%)
for how future changes get checked against it.

**Next**: Phase 5 (APU/sound) or Phase 6 (real-game validation) - both
now genuinely possible for the first time, since interrupts/timer are
what most real games actually need to be playable rather than just
bootable. The row-0/footer gap above is worth a dedicated debugging
pass whenever precise Mode-3 pixel timing becomes the active work,
rather than something to chase down mid-Phase-4.

**Phase 5 (APU/sound): done, with several genuinely obscure quirks
honestly deferred.** `src/apu.{c,h}` (new) implements all four
sound channels (two pulse, one wave, one noise), the DIV-APU frame
sequencer (512 Hz, tied to `DIV` bit 4's falling edge - the same real-
hardware-counter approach `timer.c` already uses for `DIV`/`TIMA`, not
an independent counter), CH1's sweep unit with its own shadow register,
length timers, envelope, DAC on/off, and `NR50`/`NR51`/`NR52` mixing
including the documented DMG high-pass filter. `src/mmu.c`
routes the full `0xFF10`-`0xFF3F` span (not two narrower ranges split
around `NR52`/Wave RAM, which silently missed the `0xFF27`-`0xFF2F`
gap registers - found via Blargg's `01-registers.gb`, see below) to it.
`main.c` gained `--wav`/`--seconds` flags to dump generated audio as a
standard 16-bit PCM WAV file, since `dmg_sound`'s later sub-tests
report results via the screen rather than serial output, needing
`--ppm` (viewed as a PNG) rather than grepped stdout. Every register
layout, the frame sequencer's timing, the DAC's negative-slope analog
mapping, and the high-pass filter's own cited algorithm are grounded
against pandocs' `Audio.md`/`Audio_Registers.md`/`Audio_details.md`
(fetched during this phase), not guessed.

Two of `Audio_details.md`'s "Obscure Behavior" quirks around the length
timer's interaction with the DIV-APU frame sequencer's phase are also
implemented, not just documented as deferred - found necessary (not
optional polish) by Blargg's own `03-trigger.gb` and
`08-len ctr during power.gb`, both of which use them as their actual
measurement technique for probing the length counter's otherwise-
unreadable internal state: writing `NRx4` with a 0-to-1 length-enable
transition on a frame-sequencer step that wouldn't itself have clocked
length immediately clocks it once early (`extra_length_clock_on_enable()`),
and triggering a channel under the same condition, when length is being
reloaded from zero, reloads to one below max instead of max
(`trigger_length_reload()`).

**A real, separate bug found and fixed this phase**: `01-registers.gb`
("Failed #2") caught `mmu.c`'s APU routing gap above - `0xFF27`-`0xFF2F`
(nine unused registers between `NR52` and Wave RAM) fell through to
plain flat memory instead of the APU's own read-as-`$FF`/ignore-write
handling, since the original routing was two ranges split around that
gap rather than one contiguous span. Diagnosed by building an isolated
C reproduction of the test's own register/mask table first (ruling out
an `apu.c`-only logic bug), then re-deriving the test's actual address
range from its source rather than guessing - confirmed via the real
Blargg assembly (`retrio/gb-test-roms`' `dmg_sound/source/*.s`, fetched
during this phase, the same "get the primary source, don't guess"
discipline `CLAUDE.md` already documents for the CP/M side).

**A real design correction found and fixed this phase**: an earlier
version of the `NR52` power-off handler excluded `NR11`/`NR21`/`NR31`/
`NR41` (the length-timer registers) from being zeroed, based on a
literal reading of pandocs' footnote that length timers are unaffected
by power-off on DMG. `01-registers.gb`'s test 5 (fills every register
with `$FF`, powers off, expects a full clear) proved this too broad:
`NR11`/`NR21`'s duty-cycle bits (6-7) are real, readable register bits
that *do* clear on power-off, distinct from the internal length
countdown (a separate `GBApuChannel.length_timer` field, never derived
from the raw register byte at read time) that survives. A second,
related correction: `fill_apu_regs`'s own loop (used by `08-len ctr
during power.gb`) writes every register including `NR52` last, leaving
the APU powered off by the time the test's own length-counter-loading
writes run - initially dropped entirely by the blanket "ignore every
write while off" guard, until re-checked against the same pandocs
footnote taken correctly this time: on DMG, an `NRx1` write's length-
*reload* reaches the internal counter even while powered off (bypassing
the register bank the rest of that guard protects), while the byte's
own readable bits still don't change. Both fixes were verified against
the real Blargg assembly source (`08-len ctr during power.s`'s own
comment: "On CGB, length counters are reset when powered up. On DMG,
they are unaffected, and not clocked") rather than re-guessed a third
time.

**Correctness gate**: Blargg's `dmg_sound` sub-tests (fetched from
`retrio/gb-test-roms` for testing, same ambiguous-license/not-committed
situation as `cpu_instrs` - see Phase 1's licensing note) - **7 of 12
pass**: `01-registers`, `02-len ctr`, `03-trigger`, `04-sweep`,
`06-overflow on trigger`, and `11-regs after power` all print `Passed`.
The 5 that don't are genuinely obscure, narrow hardware behaviors, not
signs of a broader problem - and are being left deferred rather than
chased indefinitely, the same call already made and documented for
dmg-acid2's remaining ~2% gap in Phase 4:

- `05-sweep details` (Failed #4, "Exiting negate mode after calculation
  disables channel"): a real, pandocs-documented CH1 sweep quirk not
  implemented - `sweep_calc()`/`tick_sweep()` in `apu.c` don't yet track
  "was negate mode used since the last trigger."
- `07-len sweep period sync` (Failed #5, "Powering up APU MODs next
  frame time with 8192"): an APU-power-on/frame-sequencer-phase-
  synchronization detail not yet root-caused - this project's frame
  sequencer resets its own step counter independent of any fixed phase
  relationship to the power-on event itself, and pandocs doesn't specify
  one explicitly enough to implement with confidence rather than guess.
- `08-len ctr during power` (Failed, checksum mismatch): partially
  root-caused this phase (see the two fixes above, both found via this
  exact test) but the final printed length-counter values are still
  consistently one tick off from what the test's checksum expects,
  even after both quirks above are correctly modeled and hand-verified
  against the test's own `get_len_a` polling algorithm
  (`retrio/gb-test-roms`' `cpu_instrs/source/common/apu.s`, the shared
  helper `dmg_sound` also uses) - the remaining gap is plausibly a
  cycle-exact frame-sequencer-phase detail in the boot-time `sync_apu`
  alignment this test relies on, not a logic error in the two quirks
  themselves.
- `09-wave read while on`, `10-wave trigger while on`,
  `12-wave write while on`: all exercise Wave RAM's real mid-playback
  corruption/lock behavior (accessing Wave RAM while CH3 is actively
  reading it doesn't behave like a normal RAM access on real hardware) -
  deliberately not modeled, and already flagged as such in `apu.h`'s own
  top-of-file comment from when this phase started, not a new gap found
  during testing.

**Next**: Phase 6 (real-game validation) - now the natural next step,
since CPU/PPU/interrupts/timer/joypad/APU all exist and a real game can
plausibly run start-to-finish for the first time. The five `dmg_sound`
gaps above are worth a dedicated pass if audio-accuracy work becomes
the active focus again, particularly `08`'s remaining one-tick
discrepancy given how close the current implementation already is.

**Phase 6 (real-game validation): a real, unmodified homebrew game
boots, plays, and merges tiles correctly.** Same convention this
project already uses on the CP/M side (`CLAUDE.md`'s stated session
convention: find a bug via real software, root-cause it against a
grounded reference, fix, add a regression test) - see
`test_roms/2048-gb/README.md` for the full story. The target was
[2048-gb](https://github.com/Sanqui/2048-gb) (zlib-licensed, committed
to `test_roms/2048-gb/` same as dmg-acid2), a complete, real
homebrew Game Boy port of the 2048 sliding-tile puzzle.

**A real bug found immediately, before the ROM would even load**: its
cartridge header declares RAM size code `0x01`, which
`gb_cart_load()`'s `ram_banks_for_code()` (`cart.c`) rejected outright -
the previous phase's own comment there called `0x01` "never used by any
real cartridge," which pandocs' `The_Cartridge_Header.md` itself
contradicts once read carefully: `0x01` is officially "Unused," but the
same page documents that "Various 'PD' ROMs... are known to use the
`$01` RAM Size tag, but this is believed to have been a mistake with
early homebrew tools, and the PD ROMs often don't use cartridge RAM at
all" - exactly this ROM's situation (cart type `0x03`,
MBC1+RAM+BATTERY, but no actual save-game behavior was ever observed in
testing). Fixed by treating code `0x01` as 0 RAM banks rather than a
load error, letting the existing zero-size RAM handling
(`gb_cart_read_ram`/`gb_cart_write_ram`) take over rather than guessing
at a nonstandard chip size.

**No real interactive input source existed at all before this
phase** - `main.c`'s bring-up driver only ever reported "nothing
pressed" (Phase 7, a real front end reading a host keyboard/controller,
was always going to be needed eventually, but real-game validation
needs *some* way to press buttons well before then). Added `--input
<script>`: a plain text file of `<frame> <BUTTON> <down|up>` lines,
timed to VBlank frame count (the same granularity a real player's
presses land on) rather than a raw instruction count, applied via
`gb_joypad_set_action`/`gb_joypad_set_direction` - the exact API
`joypad.h` already documented as "the API a future front-end or test
harness will call," unused until now. This is a scripted test harness,
not Phase 7's real thing, but it's what made this phase's validation
possible at all.

**Validation performed**: booted 2048-gb to its title screen (rendered
frame matches the game's own known title-screen layout - "2048-gb" /
credits / "Press Start!" - see `test_roms/2048-gb/README.md`),
scripted a Start press to begin a new game (two `2` tiles spawn, Score/
High score row renders correctly), then scripted `DOWN`/`RIGHT`/`DOWN`
moves - tiles visibly slid and a new tile spawned after each move, and
the final move produced a genuine merge (two `2` tiles combining into a
single `4`, with the score updating from `0` to `4` to match) - real
game logic, not just a static frame, running correctly start-to-finish
for the first time. The full run was confirmed byte-for-byte
deterministic across repeated executions (no host-timing-derived
randomness anywhere in this emulator's reset path), so `make
gameboy-2048-test` locks the post-merge frame in as a plain `cmp`
regression baseline rather than a fuzzy match.

**Next**: Phase 7 (exploratory) - a real interactive front end (GTK+Cairo
or SDL2, decided when that phase actually starts) is now the main
remaining gap between this emulator and something actually playable by
a person in real time, now that a real game has been proven to run
correctly under scripted input. Trying more real ROMs against
`--input` scripts (particularly ones exercising MBC3's RTC or deeper
save-RAM behavior, neither meaningfully exercised by 2048-gb) is also
worth doing opportunistically, without needing a dedicated phase for it.

**Phase 7 (real graphical front end): started, video + input working,
audio deliberately deferred.** `gtk/src/main.c` (new, opt-in via
`make gameboy-gtk`, same GTK4-dependency reasoning as `cpm/gtk/`) is a
real playable front end - a GTK4 window rendering the live framebuffer
through Cairo (nearest-neighbor-scaled 4x so the real 160x144 pixel
grid stays sharp) and reading real keyboard input into the joypad
(arrows = D-pad, Z/X = B/A, Enter = Start, Right Shift = Select - the
same default layout convention BGB/SameBoy use, not invented here).
Architecturally different from `cpm/gtk/`'s approach on purpose: that
one spawns the real `bin/z80` as a child process and hands a pty to a
`VteTerminal` widget, which works because CP/M output is a text/
escape-code stream a terminal widget already knows how to interpret.
The Game Boy's output is a raw pixel framebuffer, so this front end
links the core (`src/*.c`, minus `main.c`'s own competing
`main()`) directly into one binary instead - no child process, no pty,
and therefore the macOS `posix_spawn`/xzone crash documented in
`cpm/gtk/README.md` (triggered by VTE's own child-spawn path) doesn't
apply here at all. A `g_timeout_add(16, ...)` callback steps the core
one real video frame (70224 T-states) per tick and queues a redraw;
stepping itself takes microseconds, so the ~16ms timer interval is what
actually paces wall-clock speed (a documented, deliberate
approximation of the real 59.7275 Hz - see `main.c`'s own comment - in
the same spirit as `ppu.h`'s existing "Mode 3 is always 172 dots"
simplification). Manually verified stable (steady ~30% CPU, no crash,
no illegal-opcode stop) running both 2048-gb and dmg-acid2 (an
MBC-less cart, unlike 2048-gb's MBC1) for extended periods.

**Live audio output: done**, via CoreAudio's AudioQueue (macOS-specific
by deliberate choice, not oversight - the same judgment call
`cpm/gtk/src/main.c` already made using `<mach-o/dyld.h>` directly
rather than adding portability guards for a platform nothing here is
built/tested on; a portable library like SDL2 was the other option
considered, rejected to avoid a second external dependency alongside
GTK4). `src/apu.c` was already generating real samples
(`main.c --wav` proved that) - the gap was purely playback. `setup_audio()`/
`flush_audio()` (`gtk/src/main.c`) use a "push" model matched to how
sample production actually works here: `gb_apu_step()` already paces
itself off the same ~16ms GLib timer driving video (one `step_frame()`
tick = up to one real video frame's ~738 stereo sample pairs), so each
tick just hands whatever accumulated since the last one to CoreAudio as
one small buffer and resets the append position - no ring buffer or
lock-free bookkeeping needed, since there's only ever one producer and
CoreAudio's own completion callback frees each buffer once played.
Cleaned up (`AudioQueueStop`/`AudioQueueDispose`) from the same
`on_window_destroy()` handler that already stops the video timer, same
reasoning as that earlier fix. Manually verified: builds and links
clean against the `AudioToolbox` system framework (no new Homebrew
dependency), runs stably with no CoreAudio errors in the system log
across an extended 2048-gb session.

**Still not done**: Game Boy Color support - the remaining Phase 7 item,
still fully unscoped.

**Save states: done** (`src/savestate.c`/`.h`, new). Serializes
every field `gb_cpu_step()`/`gb_ppu_step()`/`gb_timer_step()`/
`gb_apu_step()` actually consume - CPU registers, the full `memory[]`
array (VRAM/WRAM/OAM/I-O-registers/HRAM), PPU/timer/joypad/APU register
and internal state, and the cartridge's banking registers, RTC, and
battery RAM - to a single file, explicitly little-endian field by field
(not a raw struct `memcpy`, which would serialize `GBCpu`/`GBCart`'s own
pointers - `memory`, `cart`, `rom`, `ram`, etc. - as meaningless
addresses, and wouldn't give a portable on-disk layout even for the
pointer-free structs). Deliberately does *not* save `GBApu`'s
`sample_buffer`/`cap`/`len` - a driver-owned output buffer, not emulated
hardware state.

Guards against the one real failure mode a save-state feature can have -
silently restoring the wrong state - two ways: `gb_savestate_load()`
checks a stored ROM size and content hash (`fnv1a()`, a real, well-known
32-bit hash chosen over a CRC32 table/zlib dependency neither of which
this project otherwise needs) against the currently-loaded cartridge
before touching anything, refusing rather than loading mismatched
banking/RTC state onto the wrong ROM; and the save/load API lives behind
`GBCpu` alone (reached through its existing `cart`/`ppu`/`timer`/
`joypad`/`apu` pointers), so there's no way to call it with mismatched
structs by construction.

Wired into both drivers: `gtk/src/main.c` binds F5 (save) / F9
(load) to `<rom path>.state` - the same key convention several existing
emulators (VBA-M, Dolphin, RetroArch's defaults) already use, not
invented here - and `src/main.c` gained `--load-state`/
`--save-state` CLI flags for scripted/test use.

Verified two ways: `tests/test_savestate.c` (new, direct
round-trip - build every struct with a distinctive value in every single
field, save, stomp everything to a *different* set of values, load, and
check every field individually came back exactly as saved, plus a
negative case confirming a ROM mismatch is refused rather than silently
loaded) and `make gameboy-savestate-test` (new, real-ROM/real-driver
round-trip through the actual `--load-state`/`--save-state` CLI flags -
run dmg-acid2 continuously to frame 2 as a baseline, separately run it
to frame 1 and save state, then in a *third*, fresh process load that
state and run one more frame; the two frame 2s come out `cmp`
byte-identical, proving the save/load round-trip is bit-exact, not just
"close enough", against a real ROM's real execution). Both new tests
pass, and the full existing regression suite (`gameboy-test`, `gameboy-
visual-test`, `gameboy-2048-test`, `gameboy-droneboy-test`, `gameboy-
tobu-test`, both RGBDS ROMs, `gameboy-gtk`) still passes byte-identically
after this change - a real, additive feature with zero observed
regressions.

**A sharper diagnosis of dmg-acid2's still-open gap, found through
this front end specifically**: watching it run continuously (rather
than capturing one still frame, all `--ppm` testing ever did) showed a
visible flicker - real, not a GTK rendering artifact. Confirmed by
instrumenting the core directly and diffing consecutive `--ppm`
captures many frames apart: the rendered image cycles through **4
distinct states** indefinitely (LCDC/SCX settle to different values at
VBlank depending on `frame_seen % 4`, e.g. `LCDC=A9,SCX=00` /
`LCDC=C9,SCX=00` / `LCDC=D1,SCX=F3` / `LCDC=D1,SCX=F3` and back), with
up to ~10,000 of 23,040 pixels differing between adjacent frames -
`make gameboy-visual-test`'s 98.04% baseline is only ever measuring
one specific point in that cycle (the `--frames 2` capture), which is
why this was never caught before. Root cause is the same one already
named above, now confirmed at a finer grain: `gb_ppu_step()`'s fixed
172-dot Mode 3 doesn't match real hardware's variable 172-289 dots
(base + `SCX & 7` scroll penalty + per-object and window-restart
fetcher-stall penalties, pandocs' `pixel_fifo.md`), so small
CPU/PPU misalignments compound across dmg-acid2's own `SCX`-driven
raster effects instead of settling into the single static image real
hardware shows. Checked pandocs' actual formula before considering a
fix and deliberately didn't attempt one: it's a genuine per-dot pixel-
FIFO simulation (fetcher steps, object-fetch cancellation, window-
restart pixel injection), not a small `SCX % 8` patch on top of the
current whole-scanline-at-once renderer - a real, separately-scoped
rewrite of `render_scanline()`/`gb_ppu_step()`'s Mode 3 handling, not
attempted here rather than guessed at partially.

**Two more real, permissively-licensed ROMs added**, found via
<https://hh.gbdev.io/> (backed by <https://github.com/gbdev/database>,
whose per-entry `game.json` names an explicit SPDX license and, for
these two, a real linked GitHub repository with its own `LICENSE` file
independently fetched and checked before committing - the same bar
`dmg-acid2`/`2048-gb` were already held to):

- **Droneboy** (`test_roms/droneboy/`, MIT) - the live-audio
  counterpart to dmg-acid2's PPU test: real, sustained multi-channel
  sound from the moment it boots, no scripted input needed, unlike
  `2048-gb`'s single ~0.05s startup blip. Its own README describes
  interactive controls (volume/duty/frequency, chords, MIDI via an
  Arduinoboy) that this project's simple `--input` script format
  didn't manage to trigger any observable change from in testing -
  documented honestly as an open, not-deeply-investigated question
  (plausibly needs a control sequence this format doesn't express, or
  genuinely expects MIDI/link-cable input) rather than claimed as
  working interaction. `make gameboy-droneboy-test` locks in a 2-second
  `--wav` capture as a byte-for-byte regression baseline.
- **Tobu Tobu Girl** (`test_roms/tobutobugirl/`, MIT) - a second
  real-game validation target alongside `2048-gb`, this time a
  well-known action/platformer rather than a puzzle game, and a
  substantially larger ROM (2Mbit vs. 2048-gb's 256Kbit). `make
  gameboy-tobu-test` locks in the rendered title screen as a
  byte-for-byte regression baseline, the same boot-stability check
  `dmg-acid2` already establishes rather than a scripted-gameplay one
  (this game's actual controls weren't reverse-engineered here).

A third candidate found the same way, a "LoFi Chiptune Beats" music
ROM, was deliberately **not** committed: the `gbdev/database` entry
claims a Zlib license but names no repository or license file to check
it against, and the actual creator's own itch.io page states no
license at all - doesn't meet the same verification bar, so left out
rather than guessed at, exactly the same reasoning that already kept
Blargg's ambiguously-licensed test ROMs out of this repo (Phase 1's
licensing note).

Along the way, found and fixed a real, separate diagnostic bug in
`gb_cart_load()` (`cart.c`): its startup message printed `GBMbcType`'s
raw enum ordinal (`GB_MBC_NONE=0, GB_MBC1=1, GB_MBC3=2, GB_MBC5=3`)
rather than the real MBC generation number, so any MBC5 cartridge
printed `mbc=3` - reading exactly like MBC3, which it isn't. The
banking logic itself was never affected, only the diagnostic text -
`mbc_type_name()` now prints the real generation number.

**"Zombie mode" volume writes: implemented (the narrow, DMG-confirmed
case), closing a gap `apu.h` had flagged since Phase 5.** Found through
real interactive use of the Phase 7 GTK front end: Droneboy's
Sweep/Square volume faders didn't respond, while Wave/Noise did.
Droneboy's own source (fetched and read directly - `src/volume.c`)
explains why, citing the technique by name: writing `NRx2` repeatedly
with the envelope in increase mode and a period of zero nudges a
channel's *live* volume by 1 each write without needing a retrigger -
"zombie mode," a real, pandocs-documented `Audio_details.md` "Obscure
Behavior." Checked pandocs before implementing anything: the *general*
zombie-mode algorithm is explicitly described as "crazy"/inconsistent
on real DMG hardware, so only the one specific case pandocs itself
confirms as reliable "on all units tested" was implemented
(`apply_zombie_mode_increment()` in `apu.c`) - the fuller CGB-02/04
algorithm would be guessing at unconfirmed DMG behavior, not grounding.
Regression-tested directly (`tests/test_apu.c`, new, 12 checks,
wired into `make gameboy-test`) rather than only through Droneboy
itself - see `test_roms/droneboy/README.md` for the full story.

**A real, standalone CPU bug found and fixed: the "HALT immediately
after EI" sub-case of the HALT bug.** Found through Tobu Tobu Girl,
left running idle in the GTK front end past ~12 seconds - a real
illegal-opcode crash after PC jumped into WRAM, unrelated to anything
else this phase touched. Root cause: this game's main loop uses the
classic `ei; halt` idiom to wait for the next interrupt, and this
emulator was applying the *generic* halt-bug handling (double-fetch
the byte after HALT) to that case, when pandocs' `halt.md` documents it
as genuinely different real hardware behavior - HALT is effectively
canceled outright when it's the delayed instruction right after `ei`;
the already-pending interrupt dispatches normally on the next step
using HALT's own (unadvanced) address as its return point, so `RETI`
naturally re-executes the same HALT once IME has genuinely caught up.
The generic-case handling instead pushed the wrong interrupt-return
address and left a stale internal flag that then double-executed the
interrupt vector's own first instruction on top of that, silently
corrupting the stack by 2 bytes - invisible until a `RETI` many
instructions later finally popped garbage. Fixed in `gb_op_ld_r_r()`/
`gb_cpu_step()` (`cpu.c`), needing one new `GBCpu` field
(`ei_delay_active`) to let the HALT handler see whether it's executing
as EI's delayed instruction - deliberately kept separate from
`ime_pending` itself rather than reusing it, since EI's own opcode
handler writes that field too. Regression-tested directly and
precisely (`tests/test_cpu.c`, new, 14 checks: exact interrupt-
return address, exact stack balance, correct re-halt on retry - not
just "this one ROM stops crashing"), wired into `make gameboy-test`.
See `test_roms/tobutobugirl/README.md` for the full
root-causing story.

**Toolchain decision: RGBDS, not a homegrown Game Boy assembler.**
Considered extending `z80asm` (`cpm/asm/src/`) with a `CPU Z80`/`CPU GB`
directive - a real, well-scoped option, since `z80asm`'s macro/
preprocessing, expression evaluator, symbol table, and generic
directives are already CPU-agnostic; only its instruction encoder
(`encode.c`) is Z80-specific, genuinely smaller in scope than the
CPU-*emulator* sharing this project already declined for the same
CPU pair (see this doc's own "Architecture decision" section). Went
with RGBDS instead: it's already the de facto standard the whole real
Game Boy homebrew scene uses - `2048-gb`, `Tobu Tobu Girl`, and
`Droneboy` (`test_roms/`) are all built with RGBDS or GBDK
(itself built on RGBDS's assembler) - so adopting it costs nothing
against real ongoing effort maintaining a second instruction set
inside `z80asm`, for what's fundamentally a means-to-an-end need
(test content), not this project's own mission. A C or Pascal
compiler was also considered and dismissed outright: GBDK (built on
SDCC) already fills that role and is what real games already
committed here are written in - a from-scratch compiler is a far
bigger undertaking than an assembler, poor ROI for generating test
content. See `rgbds/README.md` for the full reasoning and
`rgbds/examples/hello.asm` for a real, working proof: `make
gameboy-rgbds-test` assembles, links, and fixes a real RGBDS source,
then runs the result through this project's own `bin/gameboy` and
checks its actual output - confirming the whole round-trip works, not
just that RGBDS itself does. Opt-in (`brew install rgbds`), same
external-dependency reasoning as `make gameboy-gtk` - never part of
plain `make`/`make gameboy-test`.

**First real payoff: `rgbds/examples/mbc3_rtc.asm`, closing this
doc's own previously-flagged MBC3 RTC gap.** Drives the real
memory-mapped MBC3 interface directly (bank-select at `$4000`-`$5FFF`,
the latch sequence at `$6000`-`$7FFF`, the shared `$A000`-`$BFFF`
window) - a genuinely different, real-hardware-shaped way of exercising
the same logic `tests/test_cart.c`'s synthetic `GBCart`-struct
checks already cover, not a duplicate. Writes sentinel bytes into two
banked-RAM banks and all five RTC registers, latches, reads it back,
overwrites the *live* registers without re-latching (the latch stays
frozen), then re-latches (the fresh values show through) - proving
latch/live isolation and banked-RAM isolation both hold end to end
through real CPU-executed code, not just direct struct manipulation.
`make gameboy-rgbds-mbc3-test` passed on the first real run - no bugs
found this time, a clean confirmation rather than another fix. Scoped
to what's actually implemented: `cart.c`'s own comment already states
the RTC registers don't advance with real elapsed time, so this ROM
tests write/latch/read fidelity, not "does time actually pass". See
`rgbds/README.md` for the full story.

**Phase 8: Mode 3's real, variable-length timing - implemented, and a
real, honest finding about what it did and didn't fix.** Replaced the
fixed-172-dots simplification `ppu.h` had documented since Phase 3
with pandocs' `Rendering.md` "Mode 3 length" algorithm - real hardware's
confirmed, non-hedged formula (distinct from `pixel_fifo.md`'s own
admittedly-unconfirmed timing for a different interaction): `SCX & 7`
dots, a flat 6-dot window-activation penalty, and a 6-11-dot penalty
per object overlapping the scanline (including its tile-sharing and
OAM-X=0 special cases). `compute_mode3_length()` (`ppu.c`) computes
this once per scanline, at the Mode 2→3 transition; Mode 0's own
length is derived from it (`376 - mode3_dots`, matching pandocs' table
exactly), so both modes' real durations - and therefore exactly when
Mode 0/Mode 2 STAT interrupts fire - are now accurate. Deliberately
still not a full per-dot pixel-FIFO simulation: `render_scanline()`
still computes all 160 pixels at once, since duration (not literal
per-pixel FIFO mixing) is what STAT timing depends on, and building
the full FIFO state machine remained out of scope (see `pixel_fifo.md`'s
own genuinely more speculative parts, and ppu.h's updated comment).

**Verified real and active, not silently inert**: instrumented the
computed length across a real `dmg-acid2` run and confirmed genuinely
varying, non-172 values throughout the frame (215-282 dots on
window/object-heavy scanlines), driven by that ROM's own dynamic
raster effects - this is a real, working implementation, not a formula
that happens to always evaluate to the old constant.

**Honest finding: `dmg-acid2`'s own remaining gap is unchanged by this
fix** - still exactly 451 pixels (LY=0's "HELLO WORLD!" top row, and
LY=133-141's footer tail), byte-for-byte the same failing pixels as
before, confirmed by diffing the exact mismatching rows before and
after. Root cause: those specific scanlines carry zero SCX/window/
object penalty either way (checked directly), so an accurate Mode 3
*duration* was never going to move them - this **disproves** the
Phase 4 theory that blamed Mode 3's fixed length for this specific
gap, real information even though it doesn't close it. The full
regression suite (`make gameboy-test` plus every existing ROM target)
still passes byte-for-byte identically after this change - a
real, additive correctness fix with zero observed regressions, just
not the one that happens to fix `dmg-acid2`'s last mismatch. The
actual cause remains open, most plausibly needing the full pixel-FIFO
simulation this phase deliberately still didn't build.

**Repo split: this project is now its own standalone repository**,
separated from the Z80/CP-M repo it was originally developed inside.
Rationale: by this point `gameboy/` shared zero code with `cpm/` (the
"Architecture decision" section above explains why that was true from
the very start, not something that only became true later) - the only
things shared between the two were a root `Makefile`, a `bin/` build
output directory, and general-purpose `scripts/`, none of which
constitutes real coupling. Done via `git subtree split --prefix=gameboy
-b gameboy-history`, not a fresh copy or a squashed single commit -
this project's own standard of grounded, traceable history applies to
its own git history too, so the real commit-by-commit record of every
phase/fix documented throughout this file (94 total repo commits
scanned, 22 of them touching `gameboy/`) moved intact to this repo
rather than being discarded. Every in-repo path reference that used to
read `gameboy/...` (this document, `README.md`, every source comment,
every test ROM's own `README.md`) was updated to drop that now-nonexistent
prefix; comments that cited the sibling Z80/CP-M repo's own files
(`cpm/emu/src/z80.c` and similar) as real prior art or a point of
comparison were left as-is where the citation itself is still accurate
(a real precedent this project's own design was checked against), and
only reworded where the original phrasing specifically claimed
same-repo containment (e.g. "elsewhere in this repo") that's no longer
true post-split.

**Mooneye GB Test Suite adoption: a real, committable CPU/timer/
interrupt correctness gate - and a real, grounded look at what it
found.** Phase 1 flagged Mooneye
(`Gekkio/mooneye-test-suite`, MIT-licensed) as "the recommended path to
a real, committable correctness gate" for the CPU, unlike Blargg's
`cpu_instrs`/`dmg_sound` (no explicit license, fetched locally, never
committed - both phases' own licensing notes above). Deferred at the
time because it "ships as assembly source needing `rgbds` to build" -
checked against the real upstream `Makefile` while actually scoping
this work, and that belief was simply wrong: Mooneye builds with
WLA-DX (`wla-gb`/`wlalink`), not RGBDS, so adopting RGBDS later
(`rgbds/README.md`) never actually removed this blocker the way it
looked like it had. The real unblock: Mooneye's own README links
prebuilt binary ROMs, automatically built and deployed from `main` at
<https://gekkio.fi/files/mooneye-test-suite/> - fetching those sidesteps
needing a second assembler entirely, and matches this project's
existing precedent exactly (`dmg-acid2`/`2048-gb`/`droneboy`/
`tobutobugirl` are all committed prebuilt binaries, none built from
source in this repo). See `test_roms/mooneye/README.md` for the full
fetch/license/scoping story.

A curated 44-ROM "Tier 1" subset is committed - `acceptance/timer/`
(13), `acceptance/bits/` (3), `acceptance/interrupts/ie_push.gb`,
`acceptance/instr/daa.gb`, and 26 top-level CPU/interrupt-timing ROMs -
picked for being closest to functionality this project already claims
complete (`timer.c`, interrupt dispatch, DAA, instruction timing),
deliberately excluding: `boot_*`/`serial/boot_sclk_align-*` (this
emulator never executes a real boot ROM - `gb_cpu_reset()` hardcodes
post-boot register state directly, so these test something that
doesn't apply by construction); `emulator-only/mbc2/` (not
implemented); `emulator-only/mbc1/`+`mbc5/` and `acceptance/ppu/`+
`oam_dma*` (real value, left for a follow-up slice); and
`manual-only/`/`madness/`/`misc/`/`utils/` (explicitly out of scope per
Mooneye's own README). `tests/run_mooneye.py` (new) runs every
committed ROM through `bin/gameboy` and checks Mooneye's real
pass/fail serial protocol (Fibonacci `3,5,8,13,21,34` vs. `0x42`×6 -
the same SB/SC mechanism Blargg's tests already use, no new harness
code needed) against a committed per-ROM baseline, the same
floor-not-target reasoning `tests/compare_frame.py` already uses for
dmg-acid2's percentage. `make gameboy-mooneye-test` (new, opt-in, same
convention as every other real-ROM gate) wires it in.

**Real first-run results, honestly reported rather than only landing
the passing subset: 24/44 pass.** The 20 that don't aren't 20 unrelated
mysteries - grounded by reading each failing test's real `.s` source
(`gh api repos/Gekkio/mooneye-test-suite/contents/...`) rather than
guessed at, they cluster into five real, distinct, already-mostly-
understood gaps:

- **14 ROMs** (`call_timing`, `call_cc_timing`/`call_cc_timing2`,
  `jp_timing`, `jp_cc_timing`, `push_timing`, `pop_timing`,
  `ret_timing`, `ret_cc_timing`, `reti_timing`, `rst_timing`,
  `ld_hl_sp_e_timing`, `add_sp_e_timing`) all use one identical
  technique: start a real OAM DMA transfer, pad with `nop`s tuned so a
  specific M-cycle of the instruction under test lands exactly inside
  vs. just after the DMA window, then check whether that access saw
  DMA-source garbage. This is a direct, precise hit on the exact gap
  `ppu.c` already documents from Phase 3: OAM DMA here is an instant
  copy, not a real timed 160-M-cycle transfer - correct for the
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
  TIMA-overflow-reload quirk Phase 4 already partially modeled.
- **`rapid_di_ei`**: a real, separate EI-delay edge case - rapid DI/EI
  toggling with no real instruction between them must never actually
  enable interrupts, different ground than `ei_sequence`/`ei_timing`
  (both already pass).

None of these five gaps were fixed as part of this adoption - the
point of this pass was landing the real, independent, committable
correctness signal Phase 1 originally wanted (closing the "Blargg ROMs
can't be committed at all" gap), not fixing every gap the new signal
immediately found. The full existing regression suite
(`gameboy-test`, both RGBDS targets) still passes unchanged after this
addition.

**Next**: the OAM-DMA-timing cluster (14 ROMs, one real cause) is the
highest-leverage follow-up - a real timed 160-M-cycle OAM DMA transfer
(replacing the current instant-copy simplification) would plausibly
also move `acceptance/ppu/`'s and `acceptance/oam_dma*`'s not-yet-
committed Tier 2 ROMs, and is the same kind of "real per-dot timing
model" work already flagged as dmg-acid2's own remaining open gap
(Phase 8's status). The other four gaps (`ie_push`/`if_ie_registers`,
`unused_hwio-GS`, TIMA/TMA reload-window precision, `rapid_di_ei`) are
each small and narrow enough to fix independently whenever CPU/timer
correctness is the active focus again.

**Mooneye follow-up: the four small, narrow gaps fixed - 24/44 to
28/44.** Deliberately scoped to skip the OAM-DMA-timing cluster (real,
but needs the architecture change named above) and fix the four
self-contained ones instead, one at a time, each verified against its
own real Mooneye ROM (and, for the timer quirk, a new direct unit test
too):

- **`bits/unused_hwio-GS` - fixed.** Diagnosed by rendering the ROM's
  own on-screen failure report (`--ppm`, since this specific test draws
  its diagnostic to the LCD via Mooneye's `quit_inline`/tile-font
  machinery, not serial text) rather than guessing: `$FF02` (SC) was
  the first mismatch. Checked pandocs' `Serial_Data_Transfer_(Link_
  Cable).html` (bit 7 transfer-enable, bit 1 CGB-only clock-speed, bit
  0 clock-select - bits 2-6 have no function at all) and `Interrupts.
  html` (`IF`'s bits 0-4 are the five real interrupt sources, bits 5-7
  unused) to ground the exact masks, matching Mooneye's own real-
  hardware-verified `0x7E`/`0xE0` test values precisely. `P1`/`TAC`
  already forced their unused bits to `1` on read (`joypad.c`/
  `timer.c`); `SC`, `IF`, `STAT` bit 7, and several fully-unmapped
  registers (`$FF03`, `$FF08`-`$FF0E`, `$FF4C`-`$FF7F`) didn't - fixed
  in `mmu.c`/`ppu.c`. Fixing this alone also flipped
  `if_ie_registers` to passing as a side effect (its own assertions
  directly depend on `IF`'s forced-1 upper bits), confirmed by rerunning
  the full suite rather than assumed.
- **`interrupts/ie_push` - fixed.** Root-caused by reading all four
  rounds of the real `.s` source rather than guessing: real hardware
  doesn't lock in an interrupt's target vector before dispatch's PC
  push, or after both push writes - it re-reads `IE & IF` fresh right
  after the *high*-byte push specifically. If that write happens to
  land on `IE` (`$FFFF`, possible whenever `SP` is near `$0000`/`$0001`
  at dispatch time) and clobbers away the triggering bit, the interrupt
  is genuinely cancelled (`PC` ends up at `$0000`, `IF`'s bit is never
  cleared) - real, deliberate, and confirmed by hand-tracing all four of
  the ROM's rounds against pandocs' `Interrupts.md` 5-M-cycle dispatch
  sequence before touching any code. The same clobber landing on the
  *low*-byte write instead is real too but always too late - the vector
  is already committed to by then. This also exposed a second, smaller
  bug worth fixing in passing: `gb_push16()` (`cpu.c`, shared by
  `CALL`/`PUSH`/`RST`/interrupt dispatch alike) wrote the low byte
  before the high byte - backwards from real hardware's M-cycle order
  (high byte at M=2, low byte at M=3, per Mooneye's own
  `push_timing.s`/`call_timing.s`/`rst_timing.s` comments) - observably
  identical in every normal case, but the order genuinely matters for
  this exact IE-aliasing scenario. Both fixed in `gb_push16()` and
  `gb_cpu_step()`'s interrupt-dispatch block, which now re-derives the
  target vector from a fresh `IE & IF` read taken between the two push
  writes instead of deciding it up front.
- **`rapid_di_ei` - fixed.** `gb_cpu_step()` already modeled EI's real
  one-instruction-delayed enable (captured at the top of a step,
  applied at the bottom, after that step's own instruction has run) -
  but applied it *unconditionally*, even when that step's own
  instruction was `DI`, which had just set `ime=0` moments earlier in
  the very same step. The delayed enable was silently re-flipping `ime`
  back to `1` right after `DI` ran, undoing it - real hardware never
  lets interrupts turn on even momentarily in "`ei; di`" (rapid or
  otherwise). Hand-traced against all four of the ROM's rounds (two
  rapid-toggle cases expecting no interrupt at all, two "nop after ei"
  cases expecting one) to confirm the fix before writing it: a new
  `di_cancels_ei_delay` flag, set by `gb_op_di()`, lets the end-of-step
  apply skip itself for exactly this one case, while leaving the
  existing "HALT immediately after EI" special case (which depends on
  reading `ime` mid-instruction, *before* this same end-of-step apply)
  completely untouched.
- **`timer/tima_write_reloading`, `timer/tma_write_reloading` -
  partially fixed.** Grounded precisely against pandocs'
  `Timer_Obscure_Behaviour.html` "Timer overflow behavior" section
  (fetched directly, including its real M-cycle timing diagram) before
  writing anything: a TIMA write during the same M-cycle TIMA overflowed
  in ("cycle A") cancels the pending reload/interrupt outright and the
  written value sticks; a TIMA write one M-cycle later ("cycle B", while
  the reload is already in flight) is ignored outright; a *TMA* write
  during that same cycle B, however, does propagate into TIMA
  immediately. Implemented in `timer.c` and verified two ways: a new
  direct unit test (`tests/test_timer.c`, 7 new checks, all pass) for
  the parts checkable in isolation, and by rendering each Mooneye ROM's
  own on-screen diagnostic (`--ppm`) to read its real assertion
  results - 6 of these two ROMs' combined 8 assertions now pass, up
  from 0 of 8. The remaining 1 assertion each still fails; honestly
  left open rather than forced, since it's plausibly the same
  instruction-granular (writes are only ever observable at whole-
  instruction boundaries, not true per-M-cycle) limitation the OAM-DMA
  cluster above already has, not confirmed further.

Verified after each individual fix, not just at the end: full
`gameboy-test` (including the 7 new timer checks), `gameboy-visual-
test` (98.04%, unchanged), `gameboy-2048-test`, `gameboy-droneboy-
test`, `gameboy-tobu-test`, `gameboy-savestate-test`, and both RGBDS
targets all still pass byte-for-byte identically after all four fixes -
real, additive correctness improvements with zero observed regressions.

**Next**: the OAM-DMA-timing cluster (14 ROMs) remains the highest-
leverage remaining item, and now the *only* one left in this committed
Tier 1 subset - genuinely needs the architecture change already named
above, not a small patch.

**The OAM-DMA-timing rewrite: 13/14, plus a genuine re-scoping of the
14th.** The architecture change the "Next" note above flagged - real,
per-M-cycle OAM DMA, replacing the instant-copy simplification `ppu.c`
had carried since Phase 3. Modeled as a `requested -> starting ->
active` pipeline (`GBCpu.dma_request_pending`/`dma_starting_pending`/
`dma_active`/`dma_progress`, `cpu.h`), advanced one M-cycle at a time by
a new `gb_dma_tick()` (`mmu.c`) - cross-checked two ways before writing
any code, not guessed at: against Gekkio's own mooneye-gb reference
emulator (`core/src/hardware.rs`'s `OamDma`/`emulate_oam_dma` - the
actual ground truth this test suite's own `.s` sources say they were
verified against on real hardware), and independently against
`push_timing.s`'s own padding arithmetic by hand, which agreed with
mooneye-gb's model exactly. While active, OAM ($FE00-$FE9F) reads
return `$FF` and writes are dropped outright (`gb_read_byte()`/
`gb_write_byte()`, `mmu.c`) - simpler than expected, but exactly what
both the reference model and `push_timing.gb`'s own real assertions
call for.

Rather than rewrite all ~500 opcode handlers in `cpu.c` for full
per-M-cycle accuracy, only the dozen opcodes these specific ROMs
actually probe (`CALL`/`CALL cc`/`RET`/`RET cc`/`RETI`/`RST`/`PUSH rr`/
`POP rr`/`JP nn`/`JP cc`/`ADD SP,e8`/`LD HL,SP+e8`) call `gb_dma_tick()`
themselves, once per real M-cycle, matching each opcode's own M-cycle
breakdown straight from its matching `*_timing.s` file's header
comment. Every other opcode gets one lump-sum tick for its whole
T-state count instead - deliberately not per-M-cycle-precise against
DMA, but nothing needs it to be, since real code never touches OAM
directly during an active transfer (the same busy-wait-in-HRAM
convention already relied on elsewhere) and no ROM here probes any
other opcode's timing against DMA specifically. See `cpu.c`'s
`is_dma_precise_op()`.

First full run: still 0/14, all 14 identical to before. Root-caused
(not re-guessed) by tracing `push_timing.gb` against the same padding
arithmetic already hand-verified: `ldh (<DMA), a` - the exact
instruction Mooneye's own `start_oam_dma` macro uses to trigger a
transfer - was itself a lump-sum (not precise) handler, so the `$FF46`
write everything else measures relative to was landing 2 M-cycles too
early, shifting every downstream NOP-padded test by exactly that
much. Adding `gb_op_ldh_a8_a` to the precise-ticking set (despite
`$FF46` not being OAM, and despite no test directly asserting on *its*
own timing) fixed 13 of the 14 in one pass.

The 14th, `pop_timing.gb`, doesn't move - and, on a full read of its
`.s` source rather than just its header comment (the mistake that
lumped it in with the other 13 in the first place), it turns out it
was never an OAM DMA test at all. It points `SP` at the `DIV` register
and checks whether `POP`'s own reads see `DIV`'s increment depending on
which exact M-cycle they land on - the same *kind* of per-M-cycle
precision gap, but against the timer (still only advanced once per
whole instruction, in `main.c`'s run loop, not per real M-cycle), not
DMA. Left open, honestly re-scoped as its own distinct gap (and the
same root cause `timer/tima_write_reloading`/`tma_write_reloading`'s
own last unresolved assertion has, above) rather than left folded into
"needs the DMA rewrite" - that gap is closed now; this one was never
in scope for it.

Verified the same way as every fix above: full `gameboy-test`
(including a new direct unit test for `gb_dma_tick()`'s zero-init
safety, `test_cart.c`-style - see `cpu.h`'s own comment on why DMA's
pipeline uses separate present/value pairs rather than a `-1`-sentinel
`int`), `gameboy-visual-test` (98.04%, unchanged), `gameboy-2048-test`,
`gameboy-droneboy-test`, `gameboy-tobu-test`, `gameboy-savestate-test`
(extended to round-trip genuinely mid-transfer DMA state), and both
RGBDS targets all still pass byte-for-byte identically - zero observed
regressions from this project's largest architecture change since the
PPU itself. See `test_roms/mooneye/README.md`'s own "Results: the
OAM-DMA-timing rewrite" section for the full story.

**Mooneye Tier 2: `emulator-only/mbc1`/`mbc5` - 20/21, one real bug
found and fixed.** The cheaper of the two deferred follow-up slices
(see the Tier 1 adoption entry above) - independent, non-synthetic
reference ROMs against `cart.c`'s existing MBC1/MBC5 banking, on top of
`tests/test_cart.c`'s own struct-level tests. Not a clean sweep:

- **All 8 `mbc5/rom_*.gb` ROMs failed identically on the first run - a
  real bug, not a test-harness quirk.** Every one of them calls
  straight into ROMX-bank library code (`memcpy`) before ever writing
  the MBC5 bank-select register, relying on the register's real
  power-on value to already show the right bank at `$4000-7FFF`.
  Traced with a temporary, uncommitted PC-history instrument (added to
  `main.c`, reverted after use) rather than guessed at: execution
  landed on bank 0's unused-space padding (`$FF` bytes, not real code)
  at the call target and crashed into the `$0038` RST trap within ~50
  instructions of boot, every time. Root cause: `gb_cart_load()`
  (`cart.c`) left a fresh MBC5 cart's ROM bank register at its
  `memset`-zeroed `0`. That's silently correct-looking for MBC1/MBC3 (a
  documented *read-time* "bank 0 reads as bank 1" quirk covers for it
  there - see `gb_cart_read()`'s existing `if (lo == 0) lo = 1`), but
  MBC5 has no such quirk (pandocs' `MBC5.md`: "Writing 0 will indeed
  give bank 0 on MBC5, unlike other MBCs") - so a genuinely zeroed
  register really did show bank 0's content at `$4000-7FFF` instead of
  bank 1's, from the moment the ROM loaded. Confirmed the real
  power-on value is `1`, not `0`, against Gekkio's own reference
  emulator, mooneye-gb (`core/src/hardware/cartridge.rs`'s
  `Mbc5State::default()`, `romb0: 0b0000_0001`) - itself checked
  against a real MBC5 flash cartridge per this suite's own `rom_*.s`
  sources ("Results have been verified using a flash cartridge with a
  genuine MBC5 chip"), not just another emulator's guess. Fixed by
  setting `rom_bank_lo = 1` for `GB_MBC5` carts in `gb_cart_load()`
  (`cart.c`); paired with a new direct unit test
  (`tests/test_cart.c`'s `test_mbc5_default_bank_is_one()`, loading a
  synthetic MBC5 ROM through the real `gb_cart_load()` path rather than
  constructing a `GBCart` struct by hand, so it actually exercises the
  fixed code). All 8 ROMs pass after the fix; the existing full
  regression sweep (`gameboy-test`, `gameboy-visual-test` at 98.04%
  unchanged, `gameboy-2048-test`, `gameboy-droneboy-test`,
  `gameboy-tobu-test`, `gameboy-savestate-test`) still passes
  byte-for-byte identically.
- **`mbc1/multicart_rom_8Mb.gb` - not attempted, honestly scoped out
  rather than force-fixed.** A genuinely distinct MBC1 hardware
  variant, not a bug in the regular large-ROM MBC1 path `cart.c`
  already handles. MBC1M multi-game compilation carts wire bit 4 of the
  `$2000-3FFF` ROM bank register out entirely (pandocs' `MBC1.md`'s
  "MBC1M addressing diagrams" section: "From 2000-3FFF bank register
  (bit 4 unused)"), so the bank-number formula genuinely differs from
  regular MBC1's. Doing this properly needs a multicart-detection
  heuristic first (real emulators typically check for a valid Nintendo
  logo repeated at every 256 KiB boundary, since a multicart's header
  otherwise looks like an ordinary MBC1 cart) plus a distinct address
  decode once detected - a small but genuinely separate feature, not a
  register-default tweak like the MBC5 fix above, left for its own
  follow-up rather than guessed at here.
- The other 12 `mbc1/*.gb` ROMs (banking-register bit tests, RAM-bank
  tests, and ROM-size variants from 512 KiB to 16 Mb) all passed
  unmodified.

See `test_roms/mooneye/README.md`'s own "Results: Tier 2's mbc1/mbc5"
section and `tests/run_mooneye.py`'s `EXPECTED` table for the full
per-ROM baseline.

**Next**: one known, honestly-scoped-out gap remains across the
committed Mooneye subset (80/83 overall - every `acceptance/ppu/` ROM
in the suite now passes; see the "STAT read/OAM timing lag" entry
further below, the OBJ-penalty-formula entry after it, and the
"LCD-enable line 0 quirk" entry near the end, for the full current
picture, and the per-M-cycle CPU rewrite entry before all three for
the one real, deliberately-accepted regression):

- `timer/rapid_toggle.gb`/`tima_write_reloading.gb`/
  `tma_write_reloading.gb` - the same underlying "obscure TAC-toggle
  spurious-tick edge case" the per-M-cycle rewrite's own
  `rapid_toggle.gb` regression traces to; see that entry for the full
  investigation, including an independent from-scratch reimplementation
  of mooneye-gb's own reference algorithm that converged on the same
  answer this project's own code already produces - suggesting this
  may be at or near the limit of what's resolvable without an
  external, real-hardware-verified cycle trace.

**Timer M-cycle precision: attempted, reverted - a real architecture-
size finding, not a bug fix.** The "Next" note above (in its original
wording) predicted this would be "a smaller, more contained version"
of the OAM-DMA-timing rewrite - wrong, and worth recording exactly why,
since the reasoning generalizes beyond just the timer.

Built the same shape of fix DMA got: a `gb_mcycle_tick()` wrapper
ticking both DMA *and* the timer (`gb_timer_step(timer, cpu, 4)`) once
per real M-cycle, called from the same dozen-plus already-DMA-precise
opcode handlers (`is_mcycle_precise_op()`'s set, unchanged), with
`main.c`'s separate `gb_timer_step()` call removed since `gb_cpu_step()`
would now own timer advancement entirely, the same way it already owns
DMA's. `cpu->timer` being allowed to be `NULL` (`tests/test_cpu.c`'s
own minimal-dependency `GBCpu`, documented in its own comment) meant
this needed a null guard `gb_dma_tick()` never did - straightforward,
and correctly caught before it could crash that test.

First full run: **10 previously-passing Mooneye ROMs regressed**
(`halt_ime0_nointr_timing`, `halt_ime1_timing2-GS`, and 8 timer ROMs
including `rapid_toggle`/`tim00`/`tim01`/`tim10`/`tim11`/`tima_reload`)
- and `pop_timing.gb` itself *still* didn't pass. Root-caused (not
reverted blind) by instrumenting a global running total of every
T-state ever passed to `gb_timer_step()` against the sum of every
`gb_cpu_step()` return value across a full `rapid_toggle.gb` run: they
matched exactly (239,502,848 both sides, confirmed with a temporary
counter) - ruling out any double-tick or missed-tick bug in the
mechanism itself. The real cause is architectural: Gekkio's own
mooneye-gb reference (`core/src/hardware/timer.rs`'s `tac_write_cycle`
et al.) ticks the timer from **every** M-cycle of **every** instruction
(via the same `generic_mem_cycle` every register access already goes
through), not just a curated dozen - so a `TAC`/`TIMA`/`TMA` write's
own "spurious tick" check (comparing the timer's edge state
immediately before vs. after that exact write) always sees a `DIV`
counter that's precisely, continuously up to date. This project's
DMA-precision design deliberately ticks only the opcodes real ROMs
prove need it, leaving everything else on a lump-sum lull - fine for
DMA, whose own state (active/inactive, source, progress) doesn't
depend on fine-grained CPU-side counter value at all, but wrong for
the timer, whose edge-detection *is* the counter value at an exact
instant. Applying the same "only where tests prove it's needed"
scoping to the timer silently left the counter's value wrong (ahead or
behind by a handful of T-states, depending on what ran in between) at
every write to `TAC` performed by a "precise" opcode like `LDH (a8),A`
- exactly `rapid_toggle.gb`'s and `tim*.gb`'s own common setup/helper
routines' bread and butter, hence the wide, not narrow, regression
footprint.

Getting this right for real needs the timer ticked from literally
every opcode, not a curated set - i.e. the same full per-M-cycle
interleaved-stepping rewrite this project's own OAM-DMA-timing status
entries have flagged as out of scope since Phase 3. Reverted cleanly
(`git diff` empty against the OAM-DMA-timing rewrite commit,
`gameboy-test` and `rapid_toggle.gb` both reconfirmed passing) rather
than shipped half-working - the same "document honest findings even
when a fix doesn't achieve the hoped-for result" standard this
project's Phase 8 dmg-acid2 entry and the TIMA/TMA partial fix above
both already hold to. `pop_timing.gb` and the timer-related last
assertions in `tima_write_reloading.gb`/`tma_write_reloading.gb` remain
open, still honestly attributed to the same root cause, now with a
real, tried explanation for why the DMA rewrite's own approach doesn't
transfer over for free.

**Front end: GTK4 replaced with SDL2 (`sdl/`), full parity.** The
Phase 7 front end above was built on GTK4+Cairo+CoreAudio, the
sibling Z80/CP-M repo's own toolkit choice carried over without
re-examining whether it fit this project's actual output shape (a raw
pixel framebuffer, not text or widgets). Replaced outright rather than
kept alongside: SDL2's `SDL_Renderer`/`SDL_Texture` API is built around
blitting pixel buffers directly (what `draw_frame()` already did by
hand into a Cairo image surface each frame), and `SDL_QueueAudio()`
gives the same "push samples, the device plays them" model the old
CoreAudio `AudioQueue` code used - but portably, dropping the
macOS-only AudioToolbox dependency the GTK version needed for no
benefit this project actually used (nothing here relies on
Cocoa-specific behavior beyond what SDL2's own Cocoa backend already
handles internally). GTK's own advantage - native menus/dialogs - was
never used by this front end (ROM path is a CLI argument, save state
path is derived, not picked via a file dialog), so there was no real
capability lost.

`gtk/` removed entirely; `sdl/src/main.c` is a straight rewrite with
the same shape: `GB_SCREEN_WIDTH`x`GB_SCREEN_HEIGHT` framebuffer drawn
nearest-neighbor-scaled by `SCALE` (4x), the same arrows/Z/X/Enter/
Right-Shift/F5/F9 key bindings, the same "<rom>.state" save-state path
convention, and the same one-tick-per-frame main loop shape (previously
GLib's `g_timeout_add(16, ...)`, now a plain `SDL_PollEvent`/
`step_frame()`/`draw_frame()`/`SDL_Delay()` loop with the same 16ms -
~59.7Hz real DMG rate rounded to GLib's own millisecond-timer
granularity, unchanged - approximation). Build target renamed
`make gameboy-gtk` -> `make gameboy-sdl` (`brew install sdl2`, found
already present via `pkg-config` in this environment rather than
needing a fresh install). No core (`src/`) changes at all - this was a
front-end-only swap, verified via the full existing test suite (unit
tests, dmg-acid2/2048-gb/droneboy/tobu/savestate, RGBDS, Mooneye) all
still passing unmodified, plus a manual smoke test of the new binary
itself (window opens, keys move the D-pad/press A/B/Start/Select,
audio plays, F5/F9 round-trips a save state).

**`mbc1/multicart_rom_8Mb.gb`: FIXED - MBC1M multicart wiring, detected
by porting mooneye-gb's own real-hardware-verified heuristic rather
than inventing one.** The Mooneye ROM's own `.s` source says outright
"MBC1 multicarts *cannot* be detected from the header alone" (a real
1 MiB MBC1M cart's header looks identical to a regular 1 MiB MBC1
game's), so any fix needs both a detection heuristic and a distinct
address decode, not just a register tweak.

pandocs' MBC1.md "MBC1M" section documents the real-hardware wiring
difference precisely: the secondary 2-bit register lands on ROM-bank
bits 4-5 instead of the usual 5-6, and the primary 5-bit register is
truncated to its low 4 bits for banking - though (a detail pandocs
states but doesn't spell out the consequence of) the *full* 5-bit
register still feeds the existing "reads as bank 1, not bank 0" quirk,
computed *before* any multicart truncation. Confirmed byte-for-byte
against real values in the test ROM's own `expected_banks` table
(fetched from Gekkio/mooneye-test-suite): writing primary register
value 16 (0b10000) does *not* trigger the quirk even though its
truncated low nibble is 0 - only writing literal 0 does - which
`tests/test_cart.c`'s new `test_mbc1_multicart_rom_banking()` checks
directly, alongside the bits-4-5-not-5-6 placement.

For detection, pandocs only documents the identifying trait ("a
Nintendo copyright header in bank $10"), not a precise algorithm - so
`is_mbc1_multicart()` (`cart.c`) is a direct port of Gekkio's own
mooneye-gb (`core/src/config/cartridge.rs`), the same reference this
project already cross-checks other MBC behavior against, and the
concrete implementation the Mooneye ROM's own comment ("this triggers
heuristics in some emulators (e.g. mooneye-gb)") was written to
satisfy: only a real 1 MiB ROM (`rom.len() == 0x100000` - "only 8 Mbit
MBC1 multicarts exist" per both pandocs and mooneye-gb) with a valid
Nintendo logo at >=3 of its 4 256 KiB page boundaries counts,
tolerating a menu-less layout while still not misfiring on a regular
1 MiB MBC1 game, which only ever has a valid logo in page 0 - covered
by `test_mbc1_multicart_detection()`'s two synthetic ROMs (one flagged,
one not).

Checked whether the new `mbc1_multicart` field needed its own
savestate field: it doesn't, for the same reason `mbc_type`/`rom_banks`
already aren't serialized either - it's fully re-derived from the ROM
file at `gb_cart_load()` time, and `gb_savestate_load()`'s own
fingerprint check already guarantees the same ROM is loaded first (see
`savestate.c`'s own comment on that fingerprint).

Zero regressions: full existing suite (unit tests including the two
new ones above, dmg-acid2/2048-gb/droneboy/tobu/savestate, Mooneye)
all still pass, and the previously-gated
`mbc1/multicart_rom_8Mb.gb` (added to `EXPECTED` as `PASS` in
`tests/run_mooneye.py`) now passes too - **62/65** on the committed
Mooneye subset, up from 61/65.

**Tier 2's `acceptance/oam_dma*` (6 ROMs, never fetched before): FIXED,
6/6 - two more real gaps in the OAM-DMA-timing rewrite itself, found by
finally adopting the exact ROMs that rewrite was aimed at closing.**
Deferred back when the OAM-DMA-timing rewrite first landed (see that
entry above) since only the 14-ROM `*_timing` cluster had been
committed yet; picked up now that DMA is real and timed, on the
reasonable bet these ROMs would either already pass or expose a real
remaining gap in that same rewrite - the latter turned out true, twice.

`oam_dma_start.gb`/`oam_dma_timing.gb`/`oam_dma_restart.gb` all failed
initially. `oam_dma_start.gb`'s own `.s` source (fetched, not guessed
at) uses a genuinely clever mechanism to probe the exact DMA-pipeline
M-cycle boundary: it self-modifies ROM so a `jp` lands on an `LD
(HL),A` instruction sitting one byte before OAM, whose execution both
writes `$FF46` (starting DMA) *and* falls through into fetching the
next opcode from OAM itself - which reads back `$FF` (RST `$38`) once
DMA has actually gone active, versus the real `INC B` opcode still
there if it hasn't yet. `oam_dma_timing.gb` is more direct: it NOP-pads
an `LD A,(HL)` read of OAM to land exactly one T-state before vs. after
DMA's 160th and final copy. Both `LD (HL),A` and `LD A,(HL)` (and every
other `LD r,(HL)`/`LD (HL),r` opcode, 0x40-0x7F) are dispatched through
one shared handler, `gb_op_ld_r_r` (`cpu.c`) - which wasn't in
`is_dma_precise_op()`'s set, so its one real memory access got the same
"lump sum after the whole instruction" treatment as any ordinary
opcode, exactly the class of bug the OAM-DMA-timing rewrite's own
`LDH (a8),A` fix (see that entry above) already found and fixed once,
just in a different, far more commonly-executed opcode this time.
Fixed the same way: tick once immediately before the one real memory
access (only relevant when either operand is index 6, i.e. `(HL)` -
plain register-to-register moves and HALT touch no memory and don't
need it), and added `gb_op_ld_r_r` to `is_dma_precise_op()`.

`oam_dma/sources-GS.gb` failed too, but from a wholly different, real
hardware quirk, not a timing gap: DMA's own address generator has no
special case for OAM/I-O the way the CPU's normal bus decoder does, so
a source page of `$E0`-`$FF` (pandocs' `OAM_DMA_Transfer.md` documents
only `$00`-`$DF` as valid) actually reads WRAM at `$C000`-`$DFFF`
instead - real hardware's page with bit 5 (`0x20`) cleared. Confirmed
two ways: against Gekkio's own mooneye-gb (`hardware.rs`'s
`emulate_oam_dma()`, whose source-page match arms route `0xe0..=0xef`
and `0xf0..=0xff` to the exact same `work_ram.read_lower()`/
`read_upper()` calls as `0xc0..=0xcf`/`0xd0..=0xdf`), and against this
ROM's own body: it sources DMA from page `$FE`/`$FF` and asserts OAM
ends up with whatever pattern was written to `$DE00`/`$DF00`
beforehand - exactly what page-bit-5-cleared predicts. Fixed in
`gb_dma_tick()` (`mmu.c`) by masking source pages `>= 0xC0` with
`& 0xDF` before forming the source address, so the existing
`gb_read_byte()` call naturally lands on real WRAM instead.

The WRAM-mirror fix is covered directly by `tests/test_cpu.c`'s new
`test_dma_wram_mirror_source()` (three cases: page `$FE`->`$DE00`,
page `$FF`->`$DF00`, and a legitimate page `$C0` left untouched by the
masking, so the fix can't be a false generalization).
Zero regressions across the full existing suite. All 6 ROMs added to
`EXPECTED` as `PASS` - **68/71** on the committed Mooneye subset, up
from 62/65.

**`acceptance/ppu/`: 5/12 fixed - a real STAT interrupt model, plus a
free dmg-acid2 improvement (98.04% -> 99.71%).** The last never-
fetched Tier 2 slice (12 ROMs, not the 11 earlier status entries
estimated before actually listing the tarball's contents). 2 passed
immediately; 5 more needed a genuine rebuild of how `ppu.c` requests
STAT interrupts; the remaining 7 turned out to need real per-dot PPU
precision this project doesn't have yet - the same category of gap as
the timer work above, found honestly rather than attempted blind.

The old code requested a STAT interrupt unconditionally at every mode
transition whose select bit happened to be set - close to right, but
not what real hardware does. pandocs' `Interrupt_Sources.md` "INT $48
- STAT interrupt" is explicit: the 4 sources (Mode 0/1/2, LYC==LY) are
"logically ORed into a shared STAT interrupt line", and an interrupt
fires only on that line's **rising edge**, not whenever a source's own
condition is merely true. The documented consequence, "STAT blocking"
(the same page, citing this exact ROM as its own example): if one
source already holds the line high, another source's condition
becoming true doesn't produce a new edge, so no second interrupt
fires. Rebuilt around an explicit `ppu->stat_line` (`ppu.h`) persisted
across calls, recomputed by `update_stat_line()` (`ppu.c`) at every
call site that can change any of the 4 conditions - mode transitions,
the LYC comparison flag, or the STAT register's own select bits being
written - firing only on a genuine 0->1 transition. Fixes
`stat_irq_blocking.gb` directly: round 1 (enabling Mode 1 select while
already in VBlank fires an immediate edge) and round 2 (an LY==LYC
coincidence held continuously through a Mode 3->0 transition suppresses
Mode 0's own interrupt, since the line never dropped low in between)
both depend on exactly this model, not achievable with the old
per-transition-unconditional approach.

Two more real, separate quirks found and fixed alongside the line
model itself:

- **LYC's comparison flag is "constantly updated" (pandocs' `STAT.md`),
  not just at scanline boundaries** (`stat_lyc_onoff.gb`) - the old code
  only recomputed it inside `gb_ppu_step()`'s own LY-increment paths, so
  a mid-frame LYC write, or turning the LCD back on (which resets LY to
  0 and should immediately re-evaluate against it, firing a real
  interrupt if newly true), never recomputed the flag at all. Fixed by
  calling the (now interrupt-side-effect-free) `update_lyc_flag()` from
  both `gb_ppu_write_reg()`'s `$FF45` (LYC) and `$FF40` (LCDC, the
  LCD-on-transition branch) handlers, each followed by
  `update_stat_line()`. The comparison clock is deliberately left frozen
  while the LCD is off (a LYC write there does nothing until the next
  LCD-on transition) - also directly grounded in this same ROM's own
  round-by-round assertions.
- **The VBlank transition also fires the Mode 2 (OAM) STAT condition, if
  selected** (`vblank_stat_intr-GS.gb`) - not documented on pandocs'
  general `STAT.md` page, but explicit in both this ROM's own header
  comment and Gekkio's mooneye-gb (`hardware/ppu.rs`'s `switch_mode()`
  VBlank arm, which does two independent, unconditional interrupt
  requests - one for Mode 1 if selected, one for Mode 2 if selected).
  Modeled as a direct, unconditional check alongside (not through) the
  general edge-triggered line, since it's a genuine glitch independent
  of the shared line's normal mode-based tracking - real mode is about
  to become 1, not 2, at the instant this fires.

**The remaining 7** (`hblank_ly_scx_timing-GS.gb`, 4 `intr_2_*.gb`
ROMs, `lcdon_timing-GS.gb`, `lcdon_write_timing-GS.gb`) all measure
exact-cycle timing relative to mode transitions via NOP-padded
loops - `hblank_ly_scx_timing-GS.gb`'s own header states the expected
result outright: "SCX mod 8 = 0 => LY increments 51 cycles after STAT
interrupt; 1-4 => 50; 5-7 => 49." Traced this to a real, confirmed gap
via mooneye-gb's own `ppu.rs`: its `emulate()` function requests Mode
0's STAT interrupt **one T-state before** the actual Mode 3->0 switch
(`// STAT mode=0 interrupt happens one cycle before the actual mode
switch!`, its own comment), something `ppu.h`'s existing design
explicitly never modeled (mode boundaries are only checked once per
whole `gb_ppu_step()` call - i.e. once per whole CPU instruction, not
per T-state - see `ppu.h`'s own "Deliberately still *not* a full
per-dot pixel-FIFO simulation" comment, already honest about this
scope). Getting this right needs the PPU ticked from literally every
T-state, not once per instruction - the same category of architecture
gap the timer's own per-M-cycle attempt-and-revert entry above already
found and documented, not attempted here for the same reason.

A genuine, unplanned bonus from the STAT-line rebuild: `make
gameboy-visual-test`'s dmg-acid2 match rate improved from 98.04% to
**99.71%** (22589/23040 -> 22974/23040 pixels), on top of the interrupt
model becoming more accurate - not a targeted fix, just a real
consequence of STAT interrupts now firing (and blocking) correctly
during that ROM's own real STAT-driven raster tricks.

Zero regressions across the full existing suite (unit tests,
dmg-acid2/2048-gb/droneboy/tobu/savestate, RGBDS, Mooneye). All 5 fixed
ROMs added to `EXPECTED` as `PASS`, the 7 still-open ones as `FAIL` (a
real, currently-accurate baseline, not a placeholder) -
**73/83** on the committed Mooneye subset, up from 68/71.

**The per-M-cycle CPU rewrite: attempted for real this time, and it
worked - 74/83, one real regression accepted and documented.** The
"Timer M-cycle precision: attempted, reverted" entry above concluded
this needed the timer ticked from literally every opcode, not a
curated set - "the full per-M-cycle rewrite this project has been
avoiding since Phase 3." This is that rewrite, finally attempted in
full rather than reverted at the first sign of scope.

Every opcode handler in `cpu.c` - all ~35 distinct handler functions,
covering the full 256+256 (unprefixed + CB-prefixed) opcode space -
now calls a new `gb_mcycle_tick()` (`mmu.c`) once per real M-cycle it
takes, in place of the old two-tier model (a curated dozen-plus
"DMA-precise" opcodes self-ticking, everything else getting one
lump-sum tick after the whole instruction completed).
`gb_mcycle_tick()` itself just calls the existing `gb_dma_tick()`
(unchanged) plus `gb_timer_step()`/`gb_ppu_step()`/`gb_apu_step()`,
each with a fixed 4 T-states, NULL-guarded for the three optional
subsystem pointers (`tests/test_cpu.c`'s own minimal `GBCpu` leaves
them unset). `main.c`/`sdl/src/main.c`'s driver loops no longer call
`gb_ppu_step()`/`gb_timer_step()`/`gb_apu_step()` separately - they'd
double-advance every subsystem now that `gb_cpu_step()` does it
internally, the same reasoning DMA's own ticking already established.
`is_dma_precise_op()` and the lump-sum fallback loop are gone entirely
- every opcode is precise now, so the dispatcher (`fetch_and_
dispatch_ticked()`) simplified to just ticking M0 and dispatching.

Each handler's own M-cycle breakdown was derived from the official
opcode table (already this project's primary source throughout) and
hand-verified: total ticks inserted (outer M0 + however many the
handler adds) times 4 must equal the handler's own declared T-state
return value, for every opcode, taken and not-taken paths alike (`JR
cc`/`JP cc`/`CALL cc`/`RET cc`'s conditional internal cycles). The
trickiest single case was the CB-prefixed table: BIT's `(HL)` form
skips the write-back tick the other three groups (rotate/shift, RES,
SET) need, so its own real M-cycle count (12T, not 16T) had to stay
correct through the restructuring - already a known erratum this
project's own opcode table citation flagged once before (Phase 1).

Also found and fixed a real, distinct timer bug along the way, via the
same instrumentation-driven root-causing this project always uses
rather than guessing: `gb_timer_step()`'s per-T-state loop ran the
normal falling-edge check *unconditionally* every T-state, even during
a T-state where a pending TIMA-overflow reload was also resolving.
Gekkio's own mooneye-gb (`core/src/hardware/timer.rs`'s `tick_cycle()`)
treats these as strictly mutually exclusive - `if self.overflow {
reload... } else if enabled && counter_bit() { ...normal check... }` -
never both in the same cycle. Fixed by restructuring `gb_timer_step()`
(`timer.c`) the same way: the system counter still advances every
T-state regardless, but the normal edge-check only runs when no
overflow-reload is in flight.

**Net result: 74/83 on the committed Mooneye subset, up from 73/83.**

- `pop_timing.gb` (FIXED): the ROM this entire investigation was
  chasing from the start - needed exactly the per-M-cycle timer
  precision the reverted attempt correctly diagnosed as necessary, now
  actually delivered.
- `acceptance/ppu/hblank_ly_scx_timing-GS.gb` (FIXED): one of the 7
  `acceptance/ppu/` timing ROMs left open in the previous entry -
  per-M-cycle precision closed this one too, confirming the PPU side
  of the same architectural gap was real.
- `acceptance/timer/rapid_toggle.gb` (**regressed - investigated at
  length, not resolved, accepted as a known gap**): fails with `BC` off
  by exactly one "spurious tick" loop iteration (`$FFD8` where the
  assertion expects `$FFD9`) - the *exact same symptom* the original,
  much-earlier-reverted timer attempt hit, before either DMA or PPU
  were precise. That repetition is itself informative: this isn't an
  interaction bug with DMA or PPU, it's specific and isolated to the
  timer's own most obscure edge case - rapidly toggling TAC's enable
  bit on and off so that whether the underlying counter bit happens to
  be high at that exact instant determines whether an "unexpected"
  TIMA increment occurs. Investigated two ways: (1) hand-verified every
  opcode's inserted tick count against its own real T-state total -
  all correct, no arithmetic error found; (2) instrumented the exact
  `sys_counter`/TIMA/`overflow_delay` trace through the actual failing
  run and confirmed the spurious-tick mechanism itself (enable/disable
  straddling a live counter bit, causing a synthetic falling edge)
  behaves exactly as designed, clustering ticks and "stuck" stretches
  in the pattern real hardware's own design implies. The remaining
  discrepancy is a genuine, unresolved question about the *exact*
  T-state-level alignment somewhere in the boot-to-loop sequence, not
  an obviously wrong mechanism - and this specific ROM's own header is
  the one Mooneye ROM in the entire committed suite that documents
  *real hardware itself* disagreeing across revisions ("pass: DMG ABC,
  MGB, CGB, AGB, AGS; fail: DMG 0"), about as delicate an edge case as
  the suite has. `tests/run_mooneye.py`'s `EXPECTED` records this
  honestly as `FAIL` rather than silently reverting the two real fixes
  above to avoid it - the same "document honest findings even when a
  fix doesn't achieve the hoped-for result" standard this project has
  held to since Phase 8's dmg-acid2 entry.
- `test_roms/2048-gb/reference_frame.ppm` recaptured (see that ROM's
  own README.md for the full note): it seeds its tile-spawn RNG from a
  single `LDH A,(DIV)` read, which this rewrite made genuinely more
  precise - the RNG draw at that exact point changed, and with it the
  (fully deterministic, still byte-exact-reproducible) tile-spawn
  positions by frame 180. Reconfirmed by hand before recapturing: still
  exactly one merge, still score `00004`, only the board positions
  differ - the same manual verification standard the original capture
  used, not a rubber-stamped diff.

Zero regressions anywhere else: full unit test suite, dmg-acid2 (still
99.71%), Tobu Tobu Girl, Droneboy, the savestate round-trip, and RGBDS
all still pass byte-exact/as before. `acceptance/ppu/`'s remaining 6
ROMs (the 4 `intr_2_*.gb`, both `lcdon_*-GS.gb`) and `timer/
tima_write_reloading.gb`/`tma_write_reloading.gb`'s last assertion
remain open - not yet root-caused why the PPU/timer both being
per-M-cycle-precise now didn't also close these, worth a future look.

**STAT read/OAM access timing: a genuine one-M-cycle visibility lag,
found and fixed - 3 more `acceptance/ppu/` ROMs, 77/83.** The "worth a
future look" note above got exactly that: root-caused with hand-
verified T-state instrumentation (mode-transition timestamps and
NOP-padded polling-loop iteration counts, cross-checked against a
by-hand trace of the exact same instruction sequence) rather than
guessed at.

`intr_2_mode0_timing.gb`'s two test iterations (46 vs. 45 NOPs) are
built to land the ROM's own STAT poll exactly *on* vs. one M-cycle
*before* a Mode 3->0 boundary. Tracing showed both landed on the same
iteration count instead of differing by exactly one, as the ROM's own
assertions (`assert_d $01; assert_e $02`) require. The mechanism:
`gb_mcycle_tick()` (`mmu.c`) ticks the PPU and then immediately
performs the CPU's own memory access for that same M-cycle - so a STAT
register read landing on the *exact* M-cycle a mode transition occurs
sees the transition immediately. Real hardware doesn't: the transition
only becomes externally visible to a register read from the *next*
M-cycle on. First tried delaying the transition itself (changing every
`dots >= threshold` check in `gb_ppu_step()` to `dots > threshold`) -
this uniformly shifted *every* mode boundary by one M-cycle, including
the Mode 0->2 boundary the test's own HALT-based synchronization relies
on, so the relative timing between sync point and measured event never
actually changed and the test still failed. The real fix needed to be
asymmetric: a new `ppu->visible_mode` (`ppu.h`), snapshotted from
`mode` at the *start* of `gb_ppu_step()` (i.e. one M-cycle behind),
which `gb_ppu_read_reg()`'s STAT case reads instead of `mode` directly
- while interrupt-triggering logic (`update_stat_line()` and the
VBlank-quirk check) keeps using `mode` itself, unlagged, since that's
independently already correct (`stat_irq_blocking.gb`/
`vblank_stat_intr-GS.gb`/`stat_lyc_onoff.gb` all still pass unchanged).
Fixed both `intr_2_mode0_timing.gb` and `intr_2_mode3_timing.gb`.

Investigating this cluster also surfaced a second, more broadly
consequential gap: `mmu.c` only ever blocked CPU access to OAM during
an active DMA transfer. pandocs' `Rendering.md` "PPU modes" table
documents OAM as inaccessible during Modes 2 *and* 3 too - the PPU
itself is using that bus during both, independent of DMA entirely.
Added `gb_ppu_oam_blocked()` (`ppu.h`/`ppu.c`, using the same
`visible_mode` lag as the STAT fix) and wired it into both of `mmu.c`'s
OAM read/write paths - fixing `intr_2_oam_ok_timing.gb`. This needed
one companion fix to land safely: `ppu.c`'s own *internal* OAM reads
(object selection, `compute_mode3_length()`, `render_scanline()`)
previously went through the same `gb_read_byte()` the new block now
applies to, which would have made the PPU unable to read its own OAM
during Modes 2/3 - exactly the modes it needs to, to render sprites at
all. Redirected those to a new `read_oam_internal()` that reads
`cpu->memory[]` directly, bypassing every CPU-facing bus-conflict check
- the same "the PPU's own access is never blocked by logic that exists
to block the *CPU*" pattern `gb_dma_tick()`'s own destination write
already established.

Also added `stat_line` and `visible_mode` to `savestate.c`'s PPU
section (`SAVESTATE_VERSION` 2->3, `tests/test_savestate.c` updated) -
both are live PPU state a save/load round trip previously dropped
silently. `stat_line` was a pre-existing gap from the earlier
STAT-interrupt-model rework (the "a real STAT interrupt model" entry
above), found in passing while adding `visible_mode`, not something
this specific investigation was chasing.

The remaining 3 `acceptance/ppu/` ROMs are each a separate,
substantial undertaking, not variations on the fix above:
`intr_2_mode0_timing_sprites.gb` is an exhaustive 60+ case stress test
of `compute_mode3_length()`'s own OBJ-penalty formula specifically
(untouched by this fix); `lcdon_timing-GS.gb`/`lcdon_write_timing-
GS.gb` both test a documented "the PPU is late by 2 T-cycles" special
case on the very first line after LCD is enabled, which this project
has no dedicated model for at all.

Zero regressions across the full existing suite (unit tests,
dmg-acid2/2048-gb/droneboy/tobu/savestate, RGBDS, Mooneye). **77/83**
on the committed Mooneye subset, up from 74/83.

**OBJ-penalty formula and Mode 3->0 rounding: two real formula bugs
and a genuine hardware rounding rule, found and fixed -
`intr_2_mode0_timing_sprites.gb`, 78/83.** A follow-up pass picked up
the largest of the 3 ROMs left open above: an exhaustive 105-case
stress test of `compute_mode3_length()`'s OBJ-penalty formula (OBJ
count from 1 to 10, X positions spanning the full 0-255 range including
off both screen edges, and several two-group split configurations),
and how that value feeds the Mode 3->0 transition.

Its full `.s` source (`gh api repos/Gekkio/mooneye-test-suite/contents/
acceptance/ppu/intr_2_mode0_timing_sprites.s`) was fetched and hand-
decoded into every testcase's exact OBJ configuration and its expected
extra-cycle count (the ROM calibrates two NOP-padded polling loops per
testcase against the real elapsed M-cycles from a Mode 2 STAT
interrupt to the Mode 3->0 transition, the same general technique
`intr_2_mode0_timing.gb` itself uses). A from-scratch Python
reimplementation of `compute_mode3_length()`'s algorithm, run against
all 105 testcases and compared to the ROM's own expected values,
found three real, distinct gaps - not one:

1. An OBJ at OAM X==0 ("The Pixel" completely off the left edge) was
   applying its documented flat 11-dot penalty *unconditionally per
   object*, via an early `continue` that skipped the tile-dedup
   ("already considered by a previous OBJ") mechanism entirely. Real
   hardware still runs X==0 OBJs through that same dedup: multiple
   X==0 OBJs cost `11 + 6*(n-1)` dots, not `11*n` - confirmed by the
   ROM's own 2-through-10-OBJs-all-at-X==0 testcases, which assert
   exactly that formula. Fixed by letting X==0 OBJs participate in the
   normal per-tile dedup loop, with only the *wait* component (not the
   unconditional flat 6-dot fetch cost) replaced by a fixed 5 when the
   tile hasn't been considered yet - 5+6=11 reproduces the documented
   single-OBJ total exactly, while a second OBJ landing on that same
   already-considered tile now correctly pays only the flat 6.
2. An OBJ entirely off the *right* edge of the screen (OAM X>=168,
   i.e. its leftmost screen column already >=160, GB_SCREEN_WIDTH)
   was still costing a full wait+6-dot penalty despite never being
   reached by the pixel fetcher during this scanline's Mode 3 at all -
   the ROM's own obj_x=168/169 testcases are the only ones in the
   suite asserting a *zero* OBJ penalty despite objects being selected
   for the line (selection only checks Y, not X). Fixed by skipping
   such OBJs entirely (not even the flat 6), independently of the
   X==0 exception above.
3. The real rounding rule for when a computed `mode3_dots` that
   included >=1 OBJ becomes an externally-observable Mode 3->0
   transition is *not* the same `ceil(mode3_dots/4)` plain `>=` check
   an OBJ-free scanline uses - it's 1 M-cycle *earlier*. The
   comparison threshold is `mode3_dots` rounded *down* to the nearest
   whole M-cycle (`mode3_dots & ~3`), still compared with plain `>=`;
   `ppu->dots` itself keeps carrying the *unrounded* `mode3_dots`
   forward into Mode 0's own duration afterward, so the scanline's
   total 456-dot budget is unaffected - Mode 0 simply absorbs however
   many dots Mode 3 "gave back", the same way it already absorbs an
   OBJ-free scanline's own fractional-of-4 remainder. This was the
   hardest of the three to pin down: the Python model alone couldn't
   distinguish this rounding rule from two wrong ones that fit the
   ROM's own relative dataset just as well (a global constant offset
   absorbs the difference in an offline model), so it needed direct
   T-state-level tracing of the real dispatch-to-poll instruction
   sequence in a running emulation to disambiguate. Two wrong
   hypotheses were tried and rejected first this way: a flat "always
   1 M-cycle earlier" rule, and a naive "`>` instead of `>=`" strict-
   boundary rule (the wrong *direction* entirely - it makes an
   exact-multiple `mode3_dots` transition 1 M-cycle *later*, not
   earlier). Both regressed the same 4 already-passing non-sprite
   `acceptance/ppu/` ROMs, whose own OBJ-free `mode3_dots` (e.g. the
   flat 172-dot baseline) must keep the untouched plain `>=` behavior.
   New `ppu->mode3_had_obj` field (`ppu.h`/`ppu.c`, plumbed through
   `savestate.c`, `SAVESTATE_VERSION` 3->4) records whether
   `compute_mode3_length()` actually fetched >=1 OBJ for the current
   scanline, gating the rounding to exactly the cases that need it.

Zero regressions across the full existing suite (unit tests,
dmg-acid2/2048-gb/droneboy/tobu/savestate, RGBDS, Mooneye). **78/83**
on the committed Mooneye subset, up from 77/83. The remaining 2
`acceptance/ppu/` ROMs (`lcdon_timing-GS.gb`/`lcdon_write_timing-
GS.gb`) are unrelated to this fix - see the "Next" section above.

**LCD-enable line 0 quirk, an LYC comparator glitch, VRAM access
blocking implemented from scratch, and a real read/write bus-
arbitration asymmetry - four distinct fixes, found and fixed together
- `lcdon_timing-GS.gb`/`lcdon_write_timing-GS.gb`, 80/83, every
`acceptance/ppu/` ROM in the suite now passes.** A follow-up pass
picked up the last two `acceptance/ppu/` ROMs left open: both test
exactly what happens right after LCDC's LCD-enable bit is set,
sampling LY/STAT/OAM-access/VRAM-access at M-cycle-precise offsets
from the write - the read-based ROM across 3 NOP-shifted polling
passes, the write-based one via single timed writes per testcase,
covering the same offsets. Reverse-engineered entirely from both ROMs'
own `.s` source (`gh api repos/Gekkio/mooneye-test-suite/contents/
acceptance/ppu/lcdon_timing-GS.s` and `.../lcdon_write_timing-GS.s`)
and their expectation tables, cross-checked against a from-scratch
Python model built up incrementally as each new mismatch revealed a
further, previously entirely unmodeled real mechanism:

1. **Line 0 never has a real Mode 2 at all.** Immediately after
   LCD-enable, the PPU starts directly in Mode 0 for a short, fixed
   76-dot window (this project found no data pinning down *why* 76
   specifically - real hardware's own explanation isn't on pandocs,
   only that both ROMs' data requires it), then goes straight to
   Mode 3, skipping Mode 2 (OAM scan) for this one line entirely.
   Everything after that - that Mode 3's own length via the existing
   `compute_mode3_length()`, the real Mode 0 that follows it, and
   line 1 onward - is completely ordinary. New `ppu->lcd_starting`
   flag (`ppu.h`/`ppu.c`) drives a dedicated branch in
   `gb_ppu_step()`'s Mode 0 case.
2. **The LY==LYC comparison flag (STAT bit 2) has a genuine comparator
   glitch**, not just the one-M-cycle read-visibility lag the mode
   bits already have (the "STAT read/OAM timing lag" entry above): on
   the exact M-cycle LY is about to increment, the flag reads clear
   *regardless* of whether the new LY will match LYC. The ROM's own
   LYC=0 and LYC=1 variants both assert flag-clear at that same
   M-cycle - no single "old" or "new" comparison value can explain
   both simultaneously, only a genuine forced-clear, very plausibly a
   real ripple-counter artifact (LY's low bits briefly in an invalid
   transitional state to any comparator watching them combinationally)
   rather than anything deliberately designed. New
   `ppu->visible_lyc_flag` (`ppu.h`/`ppu.c`), snapshotted alongside
   `visible_mode`.
3. **VRAM access blocking during Mode 3 had never been implemented at
   all** - `gb_read_byte()`/`gb_write_byte()` (`mmu.c`) let VRAM reads
   and writes through completely unconditionally, always, a genuine
   pre-existing gap this specific ROM happened to be the first to
   require closing. New `gb_ppu_vram_blocked()` (`ppu.h`/`ppu.c`), the
   VRAM-equivalent of the already-existing `gb_ppu_oam_blocked()`,
   wired into `mmu.c` the same way. Needed the same companion fix
   `read_oam_internal()` got in the earlier OAM-blocking pass: `ppu.c`'s
   own internal VRAM reads (tile data, tile maps, object tiles) were
   redirected to a new `read_vram_internal()` that bypasses the new
   CPU-facing block - the same "the PPU's own access is never blocked
   by logic that exists to block the CPU" pattern.
4. **OAM/VRAM bus arbitration has a real, *asymmetric* one-M-cycle
   early handoff right at the Mode 2->3 boundary** - genuinely
   different for CPU reads vs. writes, and for OAM vs. VRAM, not the
   same signal read four ways. OAM writes succeed one M-cycle before
   Mode 3 becomes STAT-visible (OAM scan has already finished with the
   bus by then); VRAM reads are instead blocked one M-cycle early (the
   Mode 3 pixel fetcher has already begun claiming the bus to
   prefetch); OAM reads and VRAM writes are unaffected, following the
   plain Mode 2/3 rule with no early transition at all. This is the
   piece that took the most iteration to pin down precisely: two
   simpler hypotheses were tried and rejected first - a single unified
   "everything transitions to Mode-3-like bus behavior early" rule
   (contradicted by OAM reads staying blocked, not unblocking, at the
   same M-cycle), and initially assigning the handoff to VRAM's *write*
   side by naive analogy with OAM's write-side handoff (contradicted
   by the write-based ROM's own VRAM-write table, which shows no
   early transition at all - it's VRAM *reads*, cross-checked against
   the read-based ROM). All four read/write x OAM/VRAM combinations
   were independently confirmed against both ROMs' own data before
   landing on this split. New
   `visible_oam_read_blocked`/`visible_oam_write_blocked`/
   `visible_vram_read_blocked`/`visible_vram_write_blocked` fields
   (`ppu.h`/`ppu.c`) replace the single `visible_oam_blocked` from the
   earlier OAM-blocking pass; `gb_ppu_oam_blocked()`/
   `gb_ppu_vram_blocked()` both gained a new `is_write` parameter.

`SAVESTATE_VERSION` bumped 4->9 across this pass as each field above
was added and versioned incrementally while iterating - see git
history for the individual steps rather than treating this as one
field dump.

**dmg-acid2's own pixel-match rate went from 99.71% to a clean
100.00%** as a direct, unplanned side effect of the VRAM-blocking fix
(item 3 above) - concrete, independent confirmation this is a real
correctness fix, not just newly-passing Mooneye ROMs. The same fix
also shifted `test_roms/2048-gb/reference_frame.ppm` and
`test_roms/droneboy/reference_audio.wav` (both recaptured - see their
own `README.md`s for the details): both ROMs write to VRAM during
Mode 3 in normal operation, previously always succeeding incorrectly -
now genuinely timing-sensitive, which cascades into a different
DIV-based RNG draw (2048-gb) and a shifted audio trace (droneboy) from
that point on, the same deterministic-but-timing-shifted pattern as
the per-M-cycle rewrite's own earlier 2048-gb recapture, not
corruption - reconfirmed by hand before recapturing either.

Zero regressions across the full existing suite (unit tests,
dmg-acid2 - now *improved*, not just unregressed -
2048-gb/droneboy [both recaptured]/tobu/savestate, RGBDS, Mooneye).
**80/83** on the committed Mooneye subset, up from 78/83. Every
`acceptance/ppu/` ROM in the committed suite now passes; the only
remaining gap is the timer cluster (`rapid_toggle.gb`/
`tima_write_reloading.gb`/`tma_write_reloading.gb`) - see the "Next"
section above.

**`dmg_sound` follow-up: two of the five original gaps closed - 8/12,
up from 6/12.** With the Mooneye `acceptance/ppu/` cluster essentially
exhausted (80/83, only the timer edge case left), picked the APU's own
remaining known gap back up: Blargg's `dmg_sound` sub-tests (Phase 5's
own status entry above), re-fetched fresh from `retrio/gb-test-roms`
for this session only per this project's standing "no explicit
license, never committed" policy (Phase 1's licensing note) - verified
via each ROM's own on-screen `Passed`/`Failed #N` text (no reliable
serial-protocol or fixed-memory-address signal found this time, unlike
Mooneye's own convention, so read directly off the rendered
framebuffer the same way `05`/`06`/`11` were already being read as of
Phase 5's own original pass).

- **`05-sweep details` (FIXED)**: pandocs' `Audio_details.md` "Obscure
  Behavior" documents this precisely: "Clearing the sweep direction
  bit in NR10 after at least one sweep calculation has been made using
  the subtraction mode since the last trigger causes the channel to be
  immediately disabled." `apu.c` had never modeled this at all. New
  `sweep_negate_since_trigger` field (`apu.h`), latched by
  `sweep_calc()` itself (the one shared function both the immediate
  trigger-time calculation and `tick_sweep()`'s own periodic ones all
  go through, so a single latch point covers every real calculation
  path), reset fresh on every trigger, and checked in the `NR10` write
  handler: clearing bit 3 while it's set immediately disables CH1 if
  the flag is set.
- **`07-len sweep period sync` (FIXED)**: test 5, "Powering up APU
  MODs next frame time with 8192," asserts that several different
  power-off/power-on/delay sequences - all synchronized beforehand to
  the same real DIV-APU phase via the ROM's own `sync_apu` helper -
  produce the *same* subsequent length-clock timing after powering
  back on. That's only possible if powering on resets the frame
  sequencer's own phase to a fixed point (step 0), not if it resumes
  from wherever it was frozen when powered off - directly corroborated
  by that same ROM's test 6, literally titled "Powering up APU resets
  128 Hz sweep divider." `gb_apu_write()`'s `NR52` handler now resets
  `apu->frame_seq_step = 0` on the off-to-on transition;
  `div_bit4_prev` is deliberately left untouched, since it already
  tracks the real `DIV` register continuously regardless of power
  state (confirmed correct independently: this fix alone was
  sufficient, no `div_bit4_prev` changes needed).
- **`08-len ctr during power` (still open, but real progress)**: both
  fixes above are necessary preconditions this test also exercises
  (it interleaves power-off/power-on with length-counter timing across
  all 4 channels), and the checksum failure persists past both -
  narrowed via direct M-cycle-level trace instrumentation (the same
  successful technique the LCD-enable-quirk investigation above used)
  tracking every `frame_seq_step` tick, `NR52` write, channel trigger,
  and length-expiry event around the test's own power-cycle sequence.
  The traced values are internally plausible (e.g. CH2's post-power-on
  length-timer value matches a hand-derivation straight from its own
  `NR21` write, with no corruption or unexpected reset), and one
  candidate refinement (also resetting `div_bit4_prev` at power-on) was
  tried and found to be a no-op by construction, not a fix - it's
  already continuously synced to real `DIV` regardless of power state,
  so there's no staleness to correct there. Left open rather than
  guessed further: the remaining discrepancy is real but sufficiently
  narrow (this project's own from-scratch trace couldn't isolate it
  further without either the ROM's missing `fill_apu_regs` helper
  source - not found in any `retrio/gb-test-roms` file this session
  checked, despite being referenced by three different test files - or
  substantially more hand-simulation time than this pass budgeted).
- **`09-wave read while on`/`10-wave trigger while on`/
  `12-wave write while on` (still open, unchanged)**: all three
  exercise Wave RAM's real mid-playback corruption/lock behavior,
  deliberately not modeled - unrelated to this pass's two fixes, and
  already flagged as out of scope in `apu.h`'s own top-of-file comment
  since Phase 5.

`SAVESTATE_VERSION` bumped 9->10 for the new `sweep_negate_since_trigger`
field.

Zero regressions across the full existing suite (unit tests, dmg-acid2,
2048-gb, tobu, droneboy, savestate round-trip, RGBDS, Mooneye - all
unaffected, confirmed by re-running the full sweep after these
APU-only changes). Droneboy's own reference audio, despite being a
heavy sweep/power-cycling user by ear, stayed byte-exact against its
already-committed reference - it apparently never exercises either
specific edge case these fixes target.

**Phase 9 (Game Boy Color support): the rendering pipeline is real and
verified pixel-exact against `cgb-acid2`, deliberately scoped to a
first slice rather than full CGB hardware.** After finishing the
`dmg_sound` follow-up above, moved on to CGB support per this project's
own priority ordering (DMG stretched to its practical limit first).
Scoped up front (see `docs/GAMEBOY_ROADMAP.md`'s own plan file at the
time, and the Phase 9 bullet above) to cartridge detection, WRAM/VRAM
banking, CGB tile attributes, and color palettes - the pieces needed
for real color *rendering* - deliberately deferring double-speed mode,
HDMA/GDMA, and the infrared port to a later pass (double-speed mode was
picked up in a follow-up pass - see below). This was a from-scratch
implementation: zero existing CGB scaffolding anywhere in the codebase
beforehand (confirmed by two Explore agents surveying every CGB-adjacent
file before writing any code).

- **Mode detection and boot state**: `GBCart` gained `cgb_flag`/
  `cgb_support` (`src/cart.c`, parsed from header byte `0x0143`,
  pandocs' `The_Cartridge_Header.md`). `GBCpu.is_cgb` is the single
  runtime mode switch, resolved once at startup by the new
  `gb_resolve_cgb_mode()` (shared by both front ends via a new
  `--mode dmg|cgb|auto` flag) rather than inferred implicitly anywhere
  else - `auto` (default) picks CGB for either CGB-flag value, DMG
  otherwise; forcing `--mode dmg` on a CGB-only cart is refused with an
  error rather than attempting a best-effort render with no real-
  hardware equivalent to validate against. `gb_cpu_reset()` gained a
  real CGB post-boot register branch, sourced from pandocs'
  `Power_Up_Sequence.md`'s actual "CGB" column (fetched fresh this
  phase, not assumed to match DMG's) - `AF=0x1180`, `BC=0x0000`,
  `DE=0xFF56`, `HL=0x000D`.
- **WRAM banking (SVBK, `0xFF70`)**: `GBCpu` gained `wram_bank[6][0x1000]`
  (banks 2-7; banks 0/1 stay exactly where they've always been, in
  `cpu->memory`) and `svbk`, following the same "separate array indexed
  by bank number, no copy-in/copy-out of a flat window" pattern
  `cart.c`'s own MBC RAM banking already established, rather than
  swapping bytes into the shared flat array. `mmu.c` routes
  `0xD000-0xDFFF` (and its echo-RAM alias) to the selected bank after
  `redirect_echo()`, implementing the real "writing 0 selects bank 1"
  quirk at access time.
- **VRAM banking (VBK, `0xFF4F`) and CGB tile attributes**: `GBPpu`
  gained `vram_bank1[0x2000]` and `vbk`. `read_vram_internal()`
  (`ppu.c`) gained a `bank` parameter threaded through
  `read_tile_pixel()` and the object tile-data fetch - DMG call sites
  always pass bank 0, so DMG rendering is provably unchanged.
  `render_scanline()` gained a real `is_cgb` branch: BG/window tile
  fetches now also read a second, attribute byte from VRAM bank 1 at
  the same map address (pandocs' `Tile_Maps.md` "BG Map Attributes") -
  palette number, tile-data VRAM bank, X/Y flip (applied by pre-
  flipping `px`/`py` before the shared pixel-fetch helper, so the
  addressing math itself doesn't need to know flip happened), and a
  BG-to-OBJ priority bit. Object attributes gained the equivalent CGB
  fields (VRAM bank, 8-palette select) alongside the existing DMG-
  shared ones.
- **Color palettes and the framebuffer**: `GBPpu` gained `bg_pram[64]`/
  `obj_pram[64]` (pandocs' `Palettes.md`: 8 palettes × 4 colors × 2
  bytes RGB555 each) and `bcps`/`ocps`, with the documented Mode-3 CRAM
  read/write blocking and always-happens address auto-increment.
  **Deliberately did not unify the DMG and CGB framebuffers into one
  RGB representation**, despite that being the original plan: verified
  against `tests/compare_frame.py`'s exact-byte-equality gate that no
  5-bit-RGB555-round-trip can reproduce the DMG framebuffer's existing
  four gray levels (255/170/85/0) exactly - 256 doesn't divide evenly
  into 32 the way it divides into 4 - so unifying would have silently
  drifted every existing DMG regression reference by a few gray levels,
  a pipeline artifact rather than a real rendering change. Kept as two
  separate arrays instead (`ppu.h`'s own comment has the full
  reasoning): `framebuffer` (DMG, byte-for-byte unchanged) and a new
  `cgb_framebuffer` (CGB, packed RGB555). `render_scanline()` also
  implements CGB's real BG-to-OBJ priority rule (LCDC bit 0 as a master
  toggle rather than a display-enable bit; the 3-flag priority table
  from pandocs' `Tile_Maps.md`), CGB's independent-of-LCDC.0 window
  enable, and CGB's OAM-order-only object drawing priority (DMG's
  X-sort is skipped, not replaced, in CGB mode - `select_objects_for_
  scanline()` gained a `sort_by_x` parameter). Both front ends
  (`src/main.c`'s `--ppm`, `sdl/src/main.c`'s live texture) gained a
  CGB branch producing real RGB output (`P6` color PPM / true ARGB8888)
  alongside the unchanged DMG path.
- **Savestate**: `SAVESTATE_VERSION` bumped 10->11 for all of the above
  (`is_cgb`, `wram_bank`, `svbk`, `vram_bank1`, `vbk`, `bg_pram`,
  `obj_pram`, `bcps`, `ocps`, `cgb_framebuffer`), with matching
  `tests/test_savestate.c` round-trip coverage.
- **Verification - `cgb-acid2`: 23040/23040 pixels match (100.00%)**,
  a genuine full pass (unlike `dmg-acid2`'s still-open small gap) -
  see `test_roms/cgb-acid2/README.md`. Getting there found one real
  bug this ROM's own visual output made obvious in a way code review
  alone hadn't: the RGB555->RGB888 conversion in both front ends had
  red and blue swapped (read as `BBBBBGGGGGRRRRR` instead of pandocs'
  actual `bits 0-4 red, 10-14 blue`) - the whole face rendered cyan
  instead of yellow before the fix. Mooneye's own upstream test suite
  was checked for CGB-specific coverage beyond what's already committed
  and found thin: only boot-register tests (`boot_regs-cgb`,
  `boot_div-cgb0`/`-cgbABCDE`, `boot_hwio-C`) and two `-C` variants of
  already-covered DMG/CGB-shared tests (`misc/bits/unused_hwio-C.s`,
  `misc/ppu/vblank_stat_intr-C.s`) exist upstream - no banking/palette/
  attribute-specific coverage at all, and the boot-register ones are
  already excluded by this project's standing "no real boot ROM
  execution" policy (`test_roms/mooneye/README.md`). A real,
  permissively-licensed CGB homebrew game for byte-exact frame
  regression (mirroring `2048-gb`/`tobutobugirl`'s role for DMG) was
  searched for but not committed this pass: the one clear candidate
  found, AntonioND's `ucity`, is GPL-3.0-licensed - a real, working
  license this project just hasn't established a policy for yet
  (everything committed so far is MIT/zlib), not a licensing problem to
  route around silently. Left open rather than guessed at.
- Zero DMG regressions: the full existing suite (unit tests, Mooneye
  80/83, `dmg-acid2` 100%, `2048-gb`, `droneboy`, `tobutobugirl`,
  savestate round-trip) stayed exactly as before throughout, confirmed
  by re-running it after every major change in this phase, not just at
  the end.

**Deferred from this pass**: the infrared port (`RP`, `0xFF56`); a real
GPL-license policy decision (needed before `ucity` or similar can be
committed as a homebrew regression test). (HDMA/GDMA was picked up in a
second follow-up pass - see below.)

**Phase 9 follow-up (double-speed mode, `KEY1`/`0xFF4D`): implemented,
and the "high-risk" flag turned out to overstate the actual blast
radius.** The concern was real - double-speed mode changes how CPU
execution rate relates to `gb_mcycle_tick()` (`src/mmu.c`), the single
shared per-M-cycle backbone every opcode handler, DMA, timer, PPU, and
APU funnels through, in the same architectural neighborhood as the
timer-precision rewrite this document's own "attempted, reverted" entry
describes. But because every one of those subsystems already funnels
through that one function, the actual fix ended up small and entirely
centralized there, not a sweep through `cpu.c`'s dozens of opcode
handlers.

Grounded in pandocs' `CGB_Registers.md` "FF4D — KEY1/SPD" (fetched
fresh, not assumed) and `gbdev/hardware.inc`'s bit constants
(`B_SPD_DOUBLE=7`, `B_SPD_PREPARE=0`): `GBCpu` gained `key1` (raw
register: bit 7 current speed read-only, bit 0 "armed" read/write,
unused bits 1-6 read as 1) and `speed_switch_pause` (M-cycles remaining
in the post-switch freeze). `gb_op_stop()` (`cpu.c`) now branches on the
armed bit - real hardware performs the actual switch (flip speed bit,
auto-clear armed bit, then a fixed 2050 M-cycle freeze) when `STOP`
executes with it set; without it, the pre-existing "generic low-power
STOP never actually suspends execution" gap
(`docs/CPU_REFERENCE.md`'s documented gap) is left untouched, since it's
a genuinely different, unrelated STOP use case. `gb_cpu_step()` drains
`speed_switch_pause` one M-cycle at a time, ahead of every other check,
calling no `gb_mcycle_tick()` at all while draining (DMA/timer/PPU/APU
all frozen together - the simplest defensible reading of pandocs' "DIV
does not tick" / "the CPU is in a strange state," see the honest
simplification note below).

The one substantive behavior change is in `gb_mcycle_tick()` itself:
`gb_dma_tick()` and `gb_timer_step()` are untouched, since both are
already driven 1:1 with real CPU M-cycles with no throttling, so OAM
DMA and the Timer/DIV registers speed up 2x for free (matching
pandocs) with zero code change. `gb_ppu_step()`/`gb_apu_step()` get
half as many T-states per call while double speed is active (2 instead
of 4) - they're being *called* twice as often in real time, so halving
keeps their real-time rate constant, matching pandocs' "PPU and APU
keep operating as usual" (i.e. don't speed up).

**Known, documented simplification**: the 2050 M-cycle freeze is
modeled as a full stop of every subsystem together, not pandocs'
PPU-mode-dependent partial freeze (black pixels only if `STOP` executes
during Mode 0/1, normal rendering during Mode 3) - a one-time, ~2ms
cosmetic detail with no known test ROM to check either interpretation
against, and pandocs itself marks whether interrupts can occur during
the freeze as an open TODO.

**Verification**: no real ROM found for this - Mooneye's upstream tree
(checked via the GitHub API, not assumed from memory) has nothing
double-speed/KEY1-related, and gambatte-core's `hwtests/` has
double-speed (`_ds_`-suffixed) variants but only as raw `.asm` sources
needing a bespoke non-rgbds build toolchain with unconfirmed licensing -
out of scope, same reasoning that kept `ucity` uncommitted above.
Covered instead with direct unit tests (`tests/test_cpu.c`): KEY1 bit
masking; the armed-vs-unarmed `STOP` branch; the freeze's exact 2050-
M-cycle drain (`pc` never moves while draining, normal dispatch resumes
once fully drained); `gb_mcycle_tick()`'s resulting PPU throttle
(observed via `GBPpu.dots`, its own per-T-state counter: 4 in DMG mode,
4 in CGB normal speed, 2 in CGB double speed). `SAVESTATE_VERSION`
bumped 11->12 for `key1`/`speed_switch_pause`, with matching
`tests/test_savestate.c` coverage. Zero regressions: the full existing
suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%,
`2048-gb`, `droneboy`, `tobutobugirl`, savestate round-trip) stayed
exactly as before, plus a manual smoke check re-running the Tobu Tobu
Girl Deluxe local demo (`roms/tobudx.gb`, gitignored) - byte-identical
to its pre-change capture, confirming this real game's rendering is
completely unaffected (it evidently never arms KEY1 itself, but proves
the untouched-path guarantee end-to-end regardless).

**Phase 9 follow-up 2 (HDMA/GDMA, `0xFF51-0xFF55`): implemented.** The
other CGB piece deferred from the original Phase 9 slice, and more
consequential for real-game compatibility than double-speed mode - many
real CGB games use HBlank DMA to stream tile data mid-frame without
visible tearing.

Grounded in pandocs' `CGB_Registers.md` "LCD VRAM DMA Transfers"
(fetched fresh). `GBCpu` gained `hdma_src_hi`/`hdma_src_lo`/
`hdma_dst_hi`/`hdma_dst_lo` (pure write-only staging latches for
`HDMA1-4`, matching pandocs' explicit "[write-only]" tag - no readback
path exists on real hardware), `hdma_mode`/`hdma_active`/`hdma_src`/
`hdma_dst`/`hdma_remaining`/`hdma_block_bytes_left` (the actual transfer
state, snapshotted from the staging latches only when `HDMA5` is
written - `start_or_cancel_hdma()`, `mmu.c`). Two modes, both sharing
one underlying byte-copy mechanism: General-Purpose DMA copies the whole
transfer as one uninterruptible block; HBlank DMA copies one 16-byte
block per real HBlank (`gb_hdma_hblank_trigger()`, hooked into `ppu.c`'s
existing Mode 3->0 transition - the same transition that already drives
STAT interrupt timing, so no new PPU state machine was needed), pausing
the CPU only for each block's few M-cycles and running normally between
blocks, potentially spanning many frames.

Both share `gb_hdma_tick()` (`mmu.c`, called from `gb_mcycle_tick()`
same as `gb_dma_tick()`): a flat 2 bytes/M-cycle at normal speed, 1
byte/M-cycle at double speed - verified directly against pandocs'
literal "8 M-cycles Normal Speed / 16 fast M-cycles Double Speed"
numbers for one block in `tests/test_cpu.c`, the same "stays at
real-time rate, needs double the M-cycles at double speed" principle
already established for the PPU/APU throttle in the double-speed pass
above. `gb_cpu_step()` blocks CPU dispatch entirely while a block is
copying (mirroring the `halted`/`speed_switch_pause` pattern already in
place), but unlike the KEY1 freeze, *does* keep calling
`gb_mcycle_tick()` so DMA/timer/PPU/APU all advance in real time -
nothing in pandocs suggests HDMA/GDMA freezes other subsystems the way
the KEY1 switch's own pause does.

**A real bug caught and fixed during this pass, before it shipped**: the
first `HDMA5` read-back implementation collapsed two genuinely different
pandocs-documented states into one - "manually cancelled mid-transfer"
(bit 7 forced to 1, but bits 0-6 *still* report the remaining block
count) versus "naturally completed" (flat `$FF`). Caught by re-reading
the plan's own grounding text against the code before writing tests,
fixed in `mmu.c`'s `HDMA5` read handler to check `hdma_remaining != 0`
as the distinguishing signal (a cancelled transfer always leaves bytes
behind; a completed one never does), and directly covered by a new
`tests/test_cpu.c` assertion.

**Known, documented simplifications**: pandocs' "upon halting the CPU...
the transfer will also be halted" is modeled as "skip arming a new
HBlank block while `cpu->halted`," re-checked at the next real HBlank -
not a precise resume-the-instant-HALT-ends model. Gambatte's own hwtest
suite has several tests targeting exactly this interaction at cycle
boundaries (`hdma_late_m0halt_*`, `hdma_ei_m3halt_m0unhalt_ly_*`),
confirming it's a real, obscure nuance - those tests are raw `.asm`
needing a bespoke non-rgbds toolchain with unconfirmed licensing, out of
scope here, same reasoning that kept `ucity` and the double-speed
gambatte tests out. VRAM-as-HDMA-source (pandocs: "yet to be tested...
will cause garbage") just does a plain read, no garbage injection.
Destination-address overflow is pandocs' own open question upstream, not
specially handled either.

**Verification**: no real ROM found - same search as the double-speed
pass (Mooneye's upstream tree, gambatte-core's `hwtests/`) applies here
too. Covered with direct unit tests (`tests/test_cpu.c`): `HDMA1-4`
write-only latching; GDMA's exact 8-vs-16-M-cycle drain rate and correct
byte placement; HBlank mode's per-trigger block copying across a
multi-block transfer; mid-transfer cancellation's exact read-back
encoding (the bug above); the `HALT`-gating simplification.
`SAVESTATE_VERSION` bumped 12->13 for the ten new `hdma_*` fields (an
HBlank transfer can genuinely be mid-flight across many frames), with
matching `tests/test_savestate.c` coverage. Zero regressions: the full
existing suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2`
100%, `2048-gb`, `droneboy`, `tobutobugirl`, savestate round-trip) stayed
exactly as before. Manual smoke check re-running the Tobu Tobu Girl
Deluxe local demo (`roms/tobudx.gb`, gitignored) at both an early and a
late frame: byte-identical to its pre-HDMA captures both times - this
particular game evidently doesn't exercise HDMA in a way that affects
either checked frame (a simpler, jam-style platformer rather than one
built around raster/streaming effects), not a red flag on its own, but
worth keeping in mind that this pass's real-game validation is thinner
than double-speed mode's - closed by a real-hardware-shaped RGBDS test
ROM in a later follow-up, see below.

**Phase 9 follow-up 3 (infrared port, `RP`/`0xFF56`): implemented at
register level, deliberately not as real communication.** The last
piece deferred from the original Phase 9 slice. Real CGB hardware uses
this for wireless peer-to-peer communication between two physical
units - fundamentally a networking feature (two separate devices
exchanging data), not just a hardware register, and this project has no
peer-to-peer/networking infrastructure at all (the existing serial port
is a one-way Blargg-style debug-output hook only). Scoped, per explicit
user direction, to register-level fidelity only: `RP` behaves correctly
per pandocs so no game hangs or misbehaves poking at it, but the
receiver never detects a signal, since no real transmitting peer exists
in a single emulator process.

Grounded in pandocs' `CGB_Registers.md` "FF56 — RP" (already fetched
this session) and `gbdev/hardware.inc`'s bit constants (`RP_READ` =
bits 7-6, `B_RP_DATA_IN` = bit 1, `B_RP_LED_ON` = bit 0). `GBCpu` gained
one field, `rp` (`src/cpu.h`) - bits 7-6 (read-enable) and bit 0
(LED on/off) are plain read/write storage with no functional effect;
bit 1 (receiving) is read-only and forced to `1` on every read
regardless of what's stored or the read-enable state - the direct,
honest consequence of the register-only scope decision, not a separate
guess. `src/mmu.c` carves `0xFF56` out of the inert stub the same way
as every other CGB register this session (SVBK/KEY1/HDMA5). The
smallest of the three Phase 9 follow-ups: one field, one register, no
new state machine.

Verified with direct unit tests (`tests/test_cpu.c`): read-enable/LED
bit round-trip; bit 1 always reads `1` regardless of what's written or
the read-enable state; unused bits (5-2) always read `1`; DMG mode
always reads `0xFF` and ignores writes. `SAVESTATE_VERSION` bumped
13->14 for `rp`, with matching `tests/test_savestate.c` coverage. No
real ROM meaningfully exercises this for a single-instance emulator (any
test would just be re-checking the same register bits a direct unit
test already covers better). Zero regressions: the full existing suite
(unit tests, Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%,
`2048-gb`, `droneboy`, `tobutobugirl`, savestate round-trip) stayed
exactly as before.

**Deferred**: real peer-to-peer IR communication (would need this
project's first networking/multi-instance link infrastructure -
explicitly out of scope, same category as real link-cable multiplayer);
a real GPL-license policy decision (needed before `ucity` or similar can
be committed as a homebrew regression test).

**Phase 9 follow-up 4 (real-hardware-shaped HDMA/GDMA validation): a
purpose-built RGBDS test ROM, closing the "real-game validation is
thinner" gap the HDMA/GDMA follow-up left open.** User asked to check
whether a real game exercises HDMA for stronger validation than
`tests/test_cpu.c`'s direct/synthetic unit tests provide. The search
came up empty rather than confirming anything: `tobutobugirl-dx`
(already used as a local demo) confirmed via direct GitHub source
search to never reference `HDMA5` at all, fully explaining its earlier
byte-identical renders rather than leaving that result ambiguous.
`mills32/Parallax-effect-for-Game-Boy-Color`, found via web search,
genuinely does use `HDMA1-5` (confirmed by reading its actual assembly
source directly, not trusting a search summary) - but has no license at
all and is built with SDCC/GBDK, a toolchain this project has never
needed and hasn't set up. GBDK-2020's own `hblank_copy_vram` example
inherits the same unresolved GPL2+LE policy question already blocking
`ucity`.

Given no clean candidate, the user chose to close the gap the same way
`rgbds/examples/mbc3_rtc.asm` already closed an equivalent one for
MBC3's RTC: a small, purpose-built ROM using the toolchain this project
already has (`rgbds`), entirely this project's own code (no licensing
question), driving the real memory-mapped `0xFF51-0xFF55` registers
through genuinely CPU-executed instructions rather than a direct
function call. New `rgbds/examples/hdma.asm` (see `rgbds/README.md`'s
own entry for the full per-round breakdown), reporting results over
serial (`SB`/`SC`, the same mechanism `hello.asm`/`mbc3_rtc.asm` use):
General-Purpose DMA (the CPU genuinely blocks on the very next
instruction until the transfer completes - no explicit wait coded into
the ROM, matching real hardware exactly); HBlank DMA (explicitly polls
`HDMA5` to completion, which only succeeds if two *real* HBlank periods
actually occur - i.e. `ppu.c`'s own Mode 3->0 transition genuinely
firing `gb_hdma_hblank_trigger()`, the one thing the direct unit tests
couldn't exercise); and VRAM bank isolation (GDMAs into VRAM bank 1 at
the exact address round 1 used in bank 0, then confirms bank 0's
original bytes are untouched). All three rounds produced exactly the
expected output on the first real run - `make gameboy-rgbds-hdma-test`
locks that captured output in as the exact expected string, same
"construct the ROM, then capture its real output" approach
`mbc3_rtc.asm`'s target already used. Needs `--mode cgb` at runtime
(HDMA is CGB-only; this ROM's own header doesn't carry the CGB flag -
`gb_resolve_cgb_mode()`'s `"cgb"` mode forces it regardless). Zero
regressions: the full existing suite, including both other RGBDS
targets, stayed green throughout.

**Phase 10 (original homebrew game content): scoped, Milestone 1
(toolchain bring-up) complete.** The other long-term goal stated at
this project's own outset ("perhaps start making some games later on
too"), picked up once the CGB work above made the emulator a genuinely
capable target: full DMG, CGB color rendering, double-speed mode, HDMA
effects, save states, a real SDL front end, and (as of the RGBDS HDMA
ROM above) three working custom test ROMs proving the whole pipeline
end to end.

Three scoping decisions, all user-directed: written in **GBDK-2020
(C)** rather than RGBDS (dramatically faster to write real game logic
than hand-written SM83 assembly - real game logic, unlike the small,
focused RGBDS test ROMs above, is the deciding factor); a **grid-based
puzzle game** (turn-based, no real-time physics, tightest realistic
scope for a genuine first game); targeting **CGB-only** (matches where
this project's recent work has concentrated, no DMG fallback rendering
path to design or maintain).

**Concept**: working title "Prism" - a classic swap-adjacent-tiles
match-3 puzzle. Deliberately not a reskin of the already-committed
`test_roms/2048-gb`, and match-3 itself is a decades-old, generic genre
convention, not modeled on any single trademarked property. A 6x6 grid
of 16x16-pixel gem tiles (2x2 hardware tiles each), 4-5 distinct gem
colors each rendered through a distinct CGB BG palette (the exact
`BCPS`/`BCPD` palette RAM work from Phase 9), D-pad-moved cursor with
`A` to select/swap, automatic swap-reverts-if-no-match, gravity/refill
after a clear, chained combos from a single swap, a fixed move budget
per session, and a game-over/restart flow. Deliberately deferred to
later milestones: sound effects, SRAM high-score persistence, a real
title screen - noted up front so an early build's scope stays honestly
incomplete rather than silently so.

**GBDK-2020 licensing resolved as a non-issue for this use, distinct
from the still-open `ucity` question**: GBDK-2020 is GPLv2+LE (the
"linking exception"). Its own `LICENSE` file, fetched and quoted
directly rather than assumed, answers the only question that actually
matters here: *"What license information is required when distributing
the compiled ROM (binary) of my game? A: There is no requirement to
include or credit any of the GBDK-2020 licenses or authors."* Using
GBDK-2020 as a compiler/library to write this project's *own* original
code is a fundamentally different situation from committing a *third
party's* GPL-licensed game (`ucity`) as a test asset - the linking
exception exists specifically so code that merely links against GBDK
can ship under any license, the same model real commercial modern GB
homebrew already ships under.

**Toolchain**: no Homebrew formula exists for GBDK-2020; installed by
extracting the official prebuilt release archive
(`gbdk-macos-arm64.tar.gz`, matching this Mac's real `arm64`
architecture, confirmed via `uname -m` rather than assumed) to
`prism/toolchain/` (gitignored, not vendored - same "real third-party
toolchain" precedent `rgbds/README.md` already established). New
top-level `prism/` subproject (own `Makefile`, own `README.md`, same
shape as `sdl/` rather than folded into the root `Makefile`'s existing
target list the way the small RGBDS example `.asm` files are, since a
real game will grow its own asset pipeline as later milestones land).
Root `Makefile` gained one opt-in passthrough target,
`gameboy-prism-build` - builds via `make -C prism`, then smoke-runs the
result through this project's own `bin/gameboy --mode cgb` (same
"opt-in, external dependency, never part of plain `make`/`make
gameboy-test`" treatment `gameboy-sdl` and the RGBDS targets already
get).

**Milestone roadmap** (whole-game scope): (1) toolchain bring-up -
this pass; (2) static 6x6 grid rendering, fixed non-random gem
arrangement - tile/palette pipeline proof; (3) cursor movement + input
handling; (4) swap + match detection + clear/gravity/refill - the core
mechanic; (5) scoring, move budget, game-over/restart flow - first
genuinely *playable* build; (6) stretch, later - sound effects, SRAM
high-score persistence, a real title screen.

**Milestone 1 (this pass): done.** `prism/src/main.c` sets CGB
background palette 0's four entries all to the same bright cyan
(`RGB(0,31,31)`) and enables the display - the whole screen renders
solid cyan regardless of whatever tile data happens to be in VRAM by
default, an unambiguous, real-hardware-shaped proof that the CGB header
flag (`lcc -Wm-yC`), this emulator's CGB mode detection, and the real
color palette RAM path all work together through this brand-new
toolchain - same role `rgbds/examples/hello.asm` played for confirming
the RGBDS round-trip. `make gameboy-prism-build` doesn't just check the
build succeeds - it captures the rendered frame and asserts every pixel
is exactly solid cyan (RGB888 `(0,255,255)`, the expected result of
this emulator's own RGB555-to-RGB888 channel expansion on
`RGB(0,31,31)`), confirmed correct on the first real run. Zero
regressions: purely additive, touches no emulator source, full existing
suite unaffected.

**Milestone 2 (this pass): done.** A fixed, non-random 6x6 grid of
gem-shaped icons - `prism/src/gems.c`/`gems.h` hold one shared 16x16
diamond tile (2x2 hardware tiles, generated from a Manhattan-distance
diamond shape by a one-off local script, not hand-typed hex) recolored
through 5 CGB background palettes, `prism/src/main.c` builds the 12x12
tile-ID and attribute maps and calls `set_bkg_data`/`set_bkg_tiles`/
`set_bkg_attributes` (GBDK, `gb/gb.h`) - confirmed the CGB attribute
byte layout these expect matches exactly what this emulator already
implements (Phase 9). `gem_type = (row + col) % 5` - a deterministic
diagonal five-color stripe, chosen specifically because it's visually
unambiguous that a real 6x6 grid rendered correctly, not just "some
tiles somewhere." A real bug found and fixed along the way: the first
version only wrote the 12x12 grid region, leaving the rest of the
32x32 background tile map at its uninitialized default (tile ID 0
repeated everywhere) - fixed by explicitly filling the whole map with a
dedicated blank tile (`BLANK_TILE_ID`) before drawing the grid.
`make gameboy-prism-build` now `cmp`s the captured frame against a
committed reference (`prism/reference_m2.ppm`, captured only after
visually confirming it by eye, same discipline as the CGB rendering
work and the RGBDS HDMA ROM), not the Milestone 1 single-color
assertion, which this milestone naturally outgrew.

**A finding, deliberately not chased down within this rendering
milestone - later corrected, see the dedicated entry after Milestone
6c below**: building this exposed a reproducible timing effect,
originally suspected as a genuine bug in this project's own PPU -
capturing a frame too soon after `DISPLAY_ON` can render a stale
pre-write screen (confirmed via a savestate dump that the actual VRAM
bytes, and even the PPU's own internal `cgb_framebuffer`, already held
the fully correct data at that point - only the frame captured and
written out was stale), and bisection showed it's tied to elapsed
pre-display CPU time in general (reproduced with a plain busy-loop, no
VRAM writes at all), not to `set_bkg_tiles` call count specifically as
first suspected. **This was later investigated properly (user-directed,
after Milestone 6c) and found to be a misdiagnosis, not a PPU bug at
all** - real CGB hardware's boot ROM leaves the LCD on with white
background palettes by the time control reaches the cartridge, and
Prism's own code was writing to VRAM/tilemap/palettes without ever
disabling the display first, exactly the tearing real hardware would
also show under identical code. See the "PPU rendering-catch-up
quirk: corrected diagnosis" entry after Milestone 6c for the full,
grounded root cause and the real fix (in `prism/src/title.c`, not the
emulator). Zero regressions from Prism's own code:
full existing suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%,
`cgb-acid2` 100%, `2048-gb`, `droneboy`, `tobutobugirl`, savestate
round-trip, both other RGBDS ROMs) stayed green throughout - none of
them capture frames within the newly-discovered stale window, so
nothing else was affected.

**Milestone 3 (this pass): done.** A real per-frame game loop - the D-pad
now moves a visible cursor over the grid, still no `A`-button/select
logic (deliberately deferred to Milestone 4, which needs match-
detection-adjacent state to make sense of a selection). New
`prism/src/cursor.c`/`cursor.h`: the cursor is a hardware sprite (OBJ),
not a background-tile trick - the standard approach for an overlay that
moves independently of the tile grid underneath it, and exercises this
emulator's OAM/sprite pipeline the same way Milestone 2 exercised the
tile/palette pipeline. One shared 8x8 "corner bracket" tile forms all
four corners of a 16x16 highlight frame via hardware `S_FLIPX`/
`S_FLIPY` flip flags (`gb/gb.h`), not four separate tiles. `joypad()`
is read once per real frame and compared against the previous frame's
reading for edge-triggered movement - the D-pad moves the cursor by
exactly one cell on the frame a direction transitions from not-pressed
to pressed, not every frame it's held, clamped to the grid's `0-5`
bounds on both axes (moving past an edge is a no-op, not a wrap). New
shared `prism/src/grid.h` holds the grid-layout constants
(`GRID_SIZE`/`GRID_ORIGIN_X`/`GRID_ORIGIN_Y`) both `main.c` (drawing the
gems) and `cursor.c` (positioning the highlight) need, rather than
duplicating them.

Verified via a real scripted `--input` sequence
(`prism/input_script_m3.txt`, the same mechanism `test_roms/2048-gb`'s
own regression test already uses) moving the cursor from `(0,0)` to
`(1,1)` (right, right, down, left) - confirmed correct by eye before
locking in `prism/reference_m3.ppm` (replacing `reference_m2.ppm`,
same "capture only after visually confirming it" discipline as every
other committed reference in this project). Explicitly re-checked
Milestone 2's own discovered PPU timing quirk against this genuinely
dynamic build too, not just assumed fixed: the same input sequence
captured at frame 60 and frame 70 produced byte-identical output,
confirming real stability rather than coincidental luck at one specific
frame count. Zero regressions: the full existing suite (unit tests,
Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%, `2048-gb`, `droneboy`,
`tobutobugirl`, savestate round-trip, all three RGBDS ROMs) stayed green
throughout.

**Milestone 4 (this pass): done.** The actual match-3 mechanic - the
first milestone that makes Prism a real playable game rather than a
rendering/input demo. Larger than Milestones 1-3 combined, kept as one
pass rather than split further: swap, match-check, clear, gravity,
refill, and cascade are genuinely interdependent, and a "swap with no
match rule" checkpoint wouldn't have been a real, independently useful
milestone.

New `prism/src/board.c`/`board.h` own the actual game state - a real
`grid[GRID_SIZE][GRID_SIZE]` of gem types in WRAM, which the tile map
had never held before (Milestone 2's grid was a fixed `(row+col)%5`
formula, not real state). `board_init()` fills it with
`rand() % GEM_TYPE_COUNT` (`rand.h`'s linear-congruential `rand()`/
`initrand()`), re-rolling any cell that would complete an immediate
3-in-a-row against its already-*placed* left/upward neighbors only
(cells to the right/below aren't filled yet in row-major order - the
standard technique). Seeded once via `initrand(DIV_REG)` at a fixed
point in `main()`, before any input is read - since this emulator's
boot sequence is itself fully deterministic, `DIV_REG`'s value at that
fixed point is the same every run, so this is simultaneously "looks
random" (`rand()`'s own output genuinely varies cell-to-cell within one
playthrough) and fully reproducible run-to-run - exactly what a
`cmp`-based regression reference needs, without faking the RNG.
`board_try_swap(x1,y1,x2,y2)` swaps two cells tentatively, checks the
whole grid for any match (row and column runs of 3+), and either
reverts (no match - grid genuinely unchanged) or commits and loops
clear -> gravity -> refill -> re-check-matches until a pass finds none
- real cascades/combos fall out of that loop naturally, not a special
case. Gravity's compaction loop deliberately uses `int8_t`, not
`uint8_t`, for its counters - a bottom-to-top compaction naturally
wants to decrement past 0, which wraps to 255 on an unsigned counter, a
real and easy mistake to make here, caught during design rather than
during testing.

Selection is a second, small state machine in `main.c` alongside the
cursor: `A` selects the cursor's current cell (a small filled-square
marker sprite, a different CGB OBJ palette from the cursor's own
corner brackets - caught a real bug here, `S_PALETTE` (`gb/gb.h`) is a
plain constant for the DMG-only OBP0/OBP1 select bit, not the
function-like macro for CGB's own 0-7 palette index; `S_PAL(n)` is the
one that actually needs calling), `A` again on the same cell
deselects, `A` on an *adjacent* cell attempts `board_try_swap()` and
always deselects afterward, `A` on a non-adjacent cell just moves the
selection there instead (forgiving UX, not an error). `cursor.h` grew
`cursor_get_x()`/`cursor_get_y()` accessors and `cursor_update()` now
takes the frame's `joypad()` reading as a parameter instead of reading
it internally, so `main.c` reads it once per frame and shares it with
the new selection logic too.

**Verification went beyond "looks right by eye."** A real scripted
`--input` sequence (`prism/input_script_m4.txt`) exercises both paths
against the deterministic initial board: a swap with no match (confirmed
revert), then one that does match. That second swap cascaded through a
*second*, unplanned match - entirely from the random refill, not
anything hand-designed - and still resolved down to a stable, match-free
board. Rather than treating the wider-than-expected change as a red
flag, it was verified programmatically: sampled every cell's rendered
color from the captured frame (matching each pixel's RGB888 value back
to its closest gem palette), confirmed zero remaining 3-in-a-rows
anywhere in the resulting grid, and confirmed every row/column the swap
never touched matched the pre-swap board exactly - real confirmation
the cascade was correct game behavior, not corruption. Rebuilt from a
clean `make clean` and re-ran to confirm the result is genuinely
deterministic (identical output), not a stale build artifact. Re-checked
Milestone 2/3's own discovered PPU timing quirk against this build too:
the same sequence captured at two different frame counts past its last
input event produced byte-identical output. `SAVESTATE_VERSION`/emulator
source: untouched - zero regressions, the full existing suite (unit
tests, Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%, `2048-gb`,
`droneboy`, `tobutobugirl`, savestate round-trip, all three RGBDS ROMs)
stayed green throughout.

**Milestone 5 (this pass): done.** Scoring, a finite move budget, a
game-over state, and `Start` to restart - the first genuinely
*complete* playable build (a session with a real beginning and end,
not just a grid you can swap forever).

Deliberately **no GBDK console/font system** - its default font's tile
placement isn't something to assume without real risk of colliding
with `gems.c`'s own tile IDs `0-4` in the same VRAM bank, and this
milestone series had already found two real surprises in GBDK/CGB
specifics (Milestone 2's uninitialized tile map, Milestone 4's
`S_PALETTE`/`S_PAL` mixup). Instead, new `prism/src/hud.c`/`hud.h`: a
small custom digit tileset, ten 8x8 tiles (digits `0`-`9`, tile IDs
`5-14`, extending `gems.c`'s existing `0-4` range) rasterized from the
classic 7-segment-display convention (public domain, not copied art -
a one-off script computes which of the 7 segments are lit per digit
and fills each as a bar). `hud_init()` loads the tileset;
`hud_set_score(uint16_t)`/`hud_set_moves(uint8_t)` redraw only their
own fixed-width digit field (decimal digit extraction via repeated
`%10`/`/10`), called only when the value actually changes. Both fields
live in background row `y=0`, which the grid (`y=3-14`) never touches.

Score/moves/game-over state lives in `main.c`, alongside the existing
`selected` state - `board.c`'s job stays grid mechanics only, not
session bookkeeping. `find_matches()` now returns a `uint8_t` *count*
of cells marked instead of a boolean, and `board_try_swap()`'s return
type changed from `uint8_t` (0/1) to `uint16_t`, accumulating the
total cleared across every clear -> gravity -> refill pass in its
cascade loop (0 still means reverted/no match). `score += cleared *
10` on a committed swap; a reverted swap costs nothing. `STARTING_MOVES`
(20) -> 0 enters game-over: further `A` presses on adjacent cells no
longer call `board_try_swap()` (cursor movement and Start still work).
`Start` always restarts - mid-game or after game-over alike, no
separate gating - resetting score to 0, moves to `STARTING_MOVES`, and
generating a fresh board via `board_init()`.

**A real debugging trail, not a straight line.** After building this
milestone and running an input script extended from Milestone 4's own,
the HUD simply never updated. Two hypotheses were tested and both
ruled out empirically rather than assumed: (1) the Milestone 2 PPU
rendering-catch-up quirk (capturing a frame too soon after
`DISPLAY_ON` can show stale content) - tested at frames up to 250,
still stale; (2) real hardware's VRAM Mode 2/3 CPU-write blocking
silently dropping writes made outside VBlank - reordered `main()`'s
loop to call `vsync()` first, rebuilt, and the HUD *still* didn't
update (this reordering was reverted once ruled out - it was solving a
problem that didn't exist here). Debug instrumentation (a hardcoded
`hud_set_score(1111)` at the top of the swap branch) confirmed the HUD
drawing mechanism itself worked fine even mid-game, which narrowed the
problem to `board_try_swap()` never reaching its match path at all -
directly checking the rendered grid against both the pre-swap board and
the Milestone 4 script's *expected* board showed they no longer
matched **either** one. The actual root cause: adding `hud.c` (and
`board.c`'s own growth) shifted the exact instruction/cycle count of
code executing before `main()` calls `initrand(DIV_REG)`, which shifted
the real-time value `DIV_REG` held at that exact moment - a genuinely
different, but still fully deterministic-for-this-build, RNG seed. The
Milestone 4 script's hardcoded swap coordinates simply weren't a
match-creating swap on Milestone 5's differently-seeded board; both of
its scripted swaps reverted with zero side effects, exactly consistent
with an unchanged HUD and an unchanged grid (confirmed byte-for-byte).
Not an emulator bug and not a game-logic bug - a test-script confound,
now documented here so it doesn't get re-discovered from scratch next
time a new source file shifts a pre-`initrand()` boot path. Fixed by
deriving a new `prism/input_script_m5.txt` from this build's actual
(pixel-sampled) board layout.

Verified programmatically, not by eye: the new script's swap produces a
real vertical three-match at column 0, and the resulting frame reads
back score `"0030"` and moves `"19"` via exact 64/64-pixel digit-tile
matching, with the rest of the grid matching the expected
compaction/refill exactly and zero matches remaining anywhere - locked
in as `prism/reference_m5.ppm`. Game-over and restart were verified
separately with `STARTING_MOVES` temporarily reduced to 1 in a
throwaway build (never committed at that value): the single move is
consumed by a real match, a second swap attempt is confirmed to be a
correct no-op (score/moves unchanged), and a scripted `Start` press is
confirmed to reset score to `"0000"`, moves to `"01"` (matching the
debug budget), and generate a fresh, valid (zero pre-existing matches)
random board - then `STARTING_MOVES` was restored to 20 and the real
build re-verified byte-identical against the already-locked
`reference_m5.ppm`. Re-checked the PPU timing quirk against this build
too (frame 90 vs 110, byte-identical). Zero regressions: the full
existing suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%,
`cgb-acid2` 100%, `2048-gb`, `droneboy`, `tobutobugirl`, savestate
round-trip, all three RGBDS ROMs) stayed green throughout.

**Milestone 6a (this pass): done.** Sound effects - asked the user which
of Milestone 6's three independent stretch features (sound effects,
SRAM high-score persistence, a real title screen) to build first; SRAM
persistence and the title screen stay on the roadmap as later, separate
passes (6b/6c).

GBDK-2020 has no higher-level "play a sound effect" API - confirmed by
reading `prism/toolchain/gbdk/include/gb/hardware.h` directly (the only
header exposing sound) and `prism/toolchain/gbdk/examples/gb/sound/
sound.c` (GBDK's own register-poke demo, ported from GBSoundDemo - a
real reference for exact bit-field usage, not a music tracker). New
`prism/src/sfx.c`/`sfx.h`: four one-shot triggers, each a single
write-and-restart to one channel's registers, relying entirely on real
GB hardware envelope/sweep/length-counter auto-decay - deliberately no
per-frame service loop or note sequencer, staying within "simple
tone-generator pokes," not building a mini tracker. `sfx_play_select()`
(channel 1, no sweep, a short high blip on selecting a cell),
`sfx_play_revert()` (channel 1, a falling pitch via a real hardware
sweep in `AUD1SWEEP_DOWN` mode, on a swap that creates no match),
`sfx_play_match()` (channel 1, a rising pitch via `AUD1SWEEP_UP` -
deliberately the mirror image of `sfx_play_revert()`, same mechanism,
opposite direction, so the two read as distinct by ear), and
`sfx_play_gameover()` (channel 4 noise, a lower, longer buzz whose
volume envelope alone fades it to silence over ~0.94s, rather than the
length counter cutting it short - a different channel and timbre, not
just a different pitch, from the other three, so it reads as a
distinct "session over" event). Each channel-1 tone's exact `NR13`/
`NR14` period value was derived from pandocs' real conversion formula
(`period = 2048 - 131072/frequency`), computed once offline rather than
hand-derived - same "generate programmatically from a real formula"
discipline as the digit/gem tile bitmaps. This project's own emulator
already had all 4 sound channels implemented and independently
verified (`src/apu.c`, `tests/test_apu.c`, and `test_roms/droneboy`'s
real sustained 4-channel audio, including a genuine CH1 sweep/envelope
bug found and fixed there) - so these SFX register writes render
through already-trustworthy hardware behavior, not an unverified code
path. `main.c` hooks: `sfx_init()` once at startup; `sfx_play_select()`
on selecting a cell; `sfx_play_match()`/`sfx_play_revert()` after
`board_try_swap()` returns (still gated by the existing
`moves_remaining > 0` check); `sfx_play_gameover()` immediately after
`moves_remaining` reaches 0. No sound on `restart_game()` this pass - a
deliberate scope decision, not an oversight: the visual reset (fresh
board, zeroed HUD) is enough feedback on its own.

**Verification mirrored `gameboy-droneboy-test`'s own established
pattern** - a byte-exact `cmp` against a committed `.wav` reference -
rather than inventing a new method for this project's first game-audio
milestone. The existing `prism/input_script_m5.txt`, unchanged, drives
one cell selection and one real match-clearing swap against the
deterministic board, exercising `sfx_play_select()`/`sfx_play_match()`;
captured via `--wav --seconds 3` and verified before locking in, not
just "a file was produced" - a small Python script (the `wave` module,
zero-crossing frequency estimation per 20ms window) confirmed genuine
silence before the first scripted input event, a mid-high blip
(~744 Hz average) at the select frame, and a rising tone (~658 Hz
average, spanning 200-1050 Hz) at the match frame. Locked in as
`prism/reference_m6_sfx.wav`; `gameboy-prism-build` now runs this `--wav`
capture and `cmp` alongside its existing `--ppm` check. `sfx_play_revert()`
and `sfx_play_gameover()` aren't reachable from that exact script (it
never attempts an invalid swap, and never runs the move budget to 0),
so both were verified the same way Milestone 5 verified game-over
*logic* itself - real, but not part of the committed byte-exact
regression: a throwaway scratch capture (one extra probe swap after the
scripted sequence) showed a clearly lower-frequency event (~183 Hz
average, vs. ~658-744 Hz for select/match) confirming `sfx_play_revert()`'s
falling sweep; a `STARTING_MOVES`-reduced-to-1 throwaway build (never
committed at that value, restored to 20 and re-verified byte-identical
against both existing references afterward) showed a much longer,
low-amplitude, broadband tail (~720ms, absent from every other event)
confirming `sfx_play_gameover()`. Zero regressions: the full existing
suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%,
`2048-gb`, `droneboy`, `tobutobugirl`, savestate round-trip, all three
RGBDS ROMs) stayed green throughout.

**Milestone 6b (this pass): done.** A real title screen - asked the user
which of Milestone 6's two remaining stretch items (SRAM high-score
persistence, a real title screen) to build next; the title screen won
on lower technical risk (reuses the existing tile-rendering pattern
rather than needing a cartridge header/MBC change) and because it's a
natural place to later show a persisted high score, so building it
first means 6c (SRAM) only has to add a number to an existing screen,
not build the screen too. SRAM persistence stays on the roadmap as 6c.

New `prism/src/title.c`/`title.h`: a small custom letter tileset - not
GBDK's console/font system, the same tile-ID-collision reasoning
Milestone 5 already established for `hud.c`'s digits - covering only
the 8 distinct letters actually needed across "PRISM" and "PRESS START"
(P, R, I, S, M, E, T, A) plus a blank tile, tile IDs `15-23` right
after `hud.c`'s digit range (`5-14`). Each letterform is the standard,
widely-known 5x7 dot-matrix block-letter shape (public domain - the
same convention as the 7-segment digits, not copied art), each row's
5-bit pattern shifted into an 8-bit tile byte via a one-off Python
calculation, duplicated per row for the same 2-color (0/3) scheme
`gems.c`/`hud.c` already use. Rendered and read back (`Read`-ing the
converted PNG) before locking anything in - both lines came out legible
and correctly centered on the first real render, confirmed by eye the
same way `gems.c`'s diamond and `dmg-acid2`'s reference frame were.
`title_screen()` loads a dedicated CGB BG palette (index 5 - gems use
`0-4`) and the tileset, blanks the whole map, draws "PRISM" and "PRESS
START" centered, `SHOW_BKG`/`DISPLAY_ON`, then loops on an
edge-triggered `Start` press; on press, `wait_vbl_done()` then
`DISPLAY_OFF` (real-hardware-safe LCD-disable timing, matching this
project's own Phase 8 LCD-enable/disable timing work) before returning,
so `main()`'s own gem/HUD tile-data load never has a frame where the
tilemap points at title-tile IDs mid-overwrite with fresh bitmap data.

**Two design points were deliberate, not incidental.** `initrand(DIV_REG)`
stays the literal first line of `main()`, with `title_screen()` called
*after* it: the title screen's wait-for-Start loop is the first
genuinely player-paced (unbounded real-time) delay anywhere in Prism's
boot sequence, and the RNG seed has to be sampled before any such wait
- sampling it after would make the random board depend on how long the
player lingered on the title screen, destroying the exact
build-to-build determinism `initrand(DIV_REG)`'s placement was
originally engineered (Milestone 4) to guarantee. And the main gameplay
loop's `prev_joy` is now seeded from the real `joypad()` state at loop
entry (`uint8_t prev_joy = joypad();`), not hardcoded `0` - a real,
easy-to-miss bug caught during design rather than after a confusing
test failure: a player still physically holding Start (the same button
that just dismissed the title screen) would otherwise read as a fresh
edge on the very first game-loop iteration and silently trigger an
extra `restart_game()`, consuming an unplanned `rand()` draw and
regenerating the board the player just saw appear.

**Verification required superseding, not reusing, the existing
Milestone 5/6a script and references** - a real, worth-stating-plainly
consequence of this design: `input_script_m5.txt`'s frame numbers count
vblank frames from power-on, and the title screen now renders an
unknown number of frames before gameplay starts, shifting every
subsequent frame number - the same category of thing Milestone 5's own
debugging saga already found once (new code shifting frame-relative
assumptions). Probed empirically rather than guessed: with a `Start`
press scripted at frame 10/11, the game grid was already fully rendered
and stable by frame 15 (confirmed via `cmp` across frames 15/20/30/50,
all byte-identical) - a fast, clean transition, no re-emergence of the
Milestone 2 PPU rendering-catch-up quirk in this new shape of
transition either. New `prism/input_script_m6b.txt` scripts that same
`Start` press, then the identical relative cursor/select/swap sequence
`input_script_m5.txt` used, with every frame number shifted +20 for
generous margin past the confirmed-stable frame 15. The result:
captured at frame 110, the new script's output is **byte-identical** to
the old, pre-title-screen `reference_m5.ppm` - direct, strong
confirmation that `initrand(DIV_REG)`'s placement above works exactly
as designed, not just plausibly. New `prism/reference_m6b.ppm` and
`prism/reference_m6b_sfx.wav` (replacing `reference_m5.ppm`/
`reference_m6_sfx.wav`, same "supersede, don't accumulate" pattern
every prior milestone transition already used; confirmed stable at
frame 130 too before locking in) and `gameboy-prism-build`'s
`PRISM_SCRIPT`/`PRISM_REF`/`PRISM_WAV_REF` variables and `--frames`
value updated accordingly. Zero regressions: the full existing suite
(unit tests, Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%,
`2048-gb`, `droneboy`, `tobutobugirl`, savestate round-trip, all three
RGBDS ROMs) stayed green throughout.

**Cartridge battery-RAM persistence (Phase 2 addendum): done.** A real,
pre-existing gap surfaced while scoping Prism's own Milestone 6c: `src/
cart.h`'s `GBCart.has_battery` field was literally commented `// not
yet used (no save-to-disk persistence this phase)` - this emulator had
no mechanism for a battery-backed cartridge's external RAM to survive
between separate process runs, only the full CPU/PPU/APU/RAM
`--save-state`/`--load-state` dump (and the SDL front end's F5/F9 keys,
which call the same thing). Asked the user whether to build a "high
score" that doesn't actually persist for a real player, or fix the
underlying gap first; picked the latter - a genuine, valuable emulator
capability on its own, not just a Prism prerequisite.

Two new functions in `src/cart.c`/`cart.h`, mirroring the existing
`gb_cart_*` naming style: `gb_cart_load_ram_file()`/
`gb_cart_save_ram_file()`, both a no-op unless `has_battery && has_ram
&& ram` (matches real hardware: no battery, no persistence, ever). A
missing file (first-ever boot) is silently fine - `cart->ram` is
already zero-initialized, and callers shouldn't trust SRAM content
blindly anyway (see Prism's own magic-byte check below) - the
real-hardware-accurate practice, not an emulator shortcut. The CLI
driver (`src/main.c`) gained an opt-in `--sav <path>` flag, deliberately
*not* automatic from the ROM path, so every existing Makefile
regression target - none of which pass it - stays completely
unaffected; confirmed by actually running `gameboy-2048-test` (a real
MBC1+battery ROM) and `gameboy-rgbds-mbc3-test` afterward and checking
`git status` shows no stray files in the committed `test_roms/`
directories, not just reasoned about. The SDL front end
(`sdl/src/main.c`) gained automatic, always-on persistence instead -
`<rom>.sav`, derived exactly the way `app.save_state_path` already is -
loaded right after `gb_cart_load()`, saved at the existing clean-
shutdown point. Deliberately shutdown-triggered, not write-through on
every cart-RAM write - a real, honestly-scoped decision (a forced kill
won't persist, unlike real hardware's always-on battery), not an
oversight; an easy future enhancement if ever wanted. Three new direct
unit tests in `tests/test_cart.c` (round-trip, missing-file-is-a-no-op,
non-battery-cart-never-touches-the-file), run automatically as part of
the existing `gameboy-test` target. Manually verified end-to-end too:
capturing a real MBC3+RTC ROM's `--sav` output showed the exact bytes
('R'/'r') that ROM's own test logic writes into cartridge RAM, not a
theoretical claim. Zero regressions: the full existing suite (unit
tests including the three new cases, Mooneye 80/83, `dmg-acid2` 100%,
`cgb-acid2` 100%, `2048-gb`, `droneboy`, `tobutobugirl`, savestate
round-trip, all three RGBDS ROMs) stayed green throughout.

**Milestone 6c (this pass): done.** Prism's own use of the capability
above - the best score ever reached, shown on the title screen and
genuinely persisted across separate play sessions. `prism/Makefile`
now builds a real MBC1+RAM+BATTERY cartridge (cartridge type `0x03`,
confirmed directly against `src/cart.c`'s own header-parsing switch;
GBDK's `-Wl-yt0x03`/`-Wm-ya1` flags per `prism/toolchain/gbdk/examples/
gb/mbc3_rtc/Makefile`'s real precedent, 1 SRAM bank) instead of
`mbc=none` - confirmed after building by checking `bin/gameboy`'s own
startup log reports it, not just trusted from the flag docs. New
`prism/src/highscore.c`/`highscore.h`: 3 bytes of real SRAM
(`gb/hardware.h`'s `_SRAM[]` array, `ENABLE_RAM`/`DISABLE_RAM`-gated) -
a 1-byte magic value (`0x48`) plus the score as two explicit
little-endian bytes, explicit packing rather than a `uint16_t*` cast,
matching this codebase's general preference for simple, explicit state
manipulation. A magic-byte mismatch (including a genuinely fresh cart,
which this emulator zero-fills) means "uninitialized," resetting to 0.
`highscore_maybe_update(score)` is called after *every* scoring swap in
`main.c`, not just at game-over - more crash-resilient (data is safe
the moment it's written, closer to real always-on-battery hardware
behavior than waiting for a session-end checkpoint), and it's also what
makes a single ordinary match-clearing swap (the same one every prior
milestone's script has used, score 0 -> 30, which beats the initial 0)
enough to exercise a real SRAM write in the committed regression test -
no throwaway-build trick needed here, unlike Milestone 5/6a's
game-over-path asymmetry.

`title.c`'s `title_screen()` gained a third line, "HIGH" plus the
4-digit value below it - two more letter glyphs (H, G, tile IDs
`24-25`, same standard 5x7 dot-matrix convention as Milestone 6b's
other eight) plus the number reusing `hud.c`'s exact digit bitmap data
(now exported via `hud.h` as `hud_digit_tiles`) at a fresh tile-ID
range (`26-35`), avoiding art duplication entirely.

**A real bug found and fixed along the way, not papered over.** Adding
the "HIGH <score>" line (row 15, just below the 12x12 grid's own rows
3-14) exposed that `board.c`'s `fill_screen_blank()` only ever reset
background *tile IDs* for the whole 32x32 map, never CGB *attributes*
(palette index) - harmless before now, since nothing had ever set a
non-zero attribute outside the grid before `board_init()` ran.
`title_screen()` became the first code to draw text there in its own
dark-navy `TITLE_PALETTE` before the game screen loads, so those cells
kept that stale palette assignment into actual gameplay - a visibly
darker patch under the grid, since `board_redraw()` only re-attributes
the grid's own 12x12 region. Found by diffing the new build's captured
frame against the still-valid Milestone 6b reference pixel-by-pixel
(not eyeballing "looks wrong"): the diff was exactly 32x8 pixels at
tile row 15, columns 8-11, pinpointing the cause immediately. Fixed by
having `fill_screen_blank()` reset attributes to 0 for the whole map
too - the same discipline that already existed for tile IDs, now
applied consistently.

A second real, expected finding: adding `highscore_init()`'s few extra
pre-title-screen instructions shifted every subsequent audio event's
exact real-time/sample position by a small, consistent amount (~40ms) -
the same category of thing Milestone 5's own RNG-seed-timing discovery
already established, just in the audio domain this time. Not enough to
shift which vblank *frame number* any scripted input event lands on
(the visual `.ppm` reference and game logic are completely unaffected -
confirmed byte-identical to the Milestone 6b reference), but enough to
fail a byte-exact `cmp` against audio captured before the shift
existed. Re-captured, re-verified (the same zero-crossing frequency
analysis confirmed both tones' actual content was identical, just
time-shifted), and superseded as `reference_m6c_sfx.wav`.

**Verified with two real, separate process invocations sharing one
`.sav` file** - genuinely exercising the same load/save path a real
player's session would, not just unit-testing the mechanism in
isolation. Run 1 (`--input input_script_m6b.txt --sav <out>.sav`)
starts fresh, makes the usual match-clearing swap, and its `.sav` is
`cmp`'d against a new committed `prism/reference_m6c.sav` (a
mostly-zero 8 KiB file - magic `0x48` + score `30` as bytes
`0x1E,0x00`, everything else genuinely zero). Run 2 (`--sav <out>.sav`,
no `--input` at all) boots straight to the title screen using that
exact file and confirms it now reads "HIGH 0030" instead of "HIGH
0000", `cmp`'d against a new committed `prism/reference_m6c_title.ppm`
(rendered and read back to confirm "HIGH" and the digits are legible
and correctly positioned before locking in, same discipline Milestone
6b used for its own new letterforms). Both steps added to
`gameboy-prism-build`'s Makefile recipe. Zero regressions: the full
existing suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%,
`cgb-acid2` 100%, `2048-gb`, `droneboy`, `tobutobugirl`, savestate
round-trip, all three RGBDS ROMs) stayed green throughout.

**PPU rendering-catch-up quirk: corrected diagnosis and fix (this
pass).** User-directed: investigate the open PPU timing finding from
Milestone 2, carried as unresolved through every subsequent milestone's
own "re-checked, still consistent" note. **It was never a PPU bug.**
Confirmed via direct instrumentation (a temporary `fprintf` at every
`frame_ready` event in `src/main.c`, reverted after use) rather than
further speculation:

```
frame=1    lcdc=0x91  bg_pram0=0x7FFF   boot-ROM state: LCD on, white
frame=2-5  lcdc=0xC0  bg_pram0=0x7FFF   still white (VRAM/palette still blank)
frame=6    lcdc=0xC0  pc=0x115B         TORN - captured mid-write, inside
                                        title_screen()'s own set_bkg_data/
                                        set_bkg_tiles/set_bkg_palette calls
frame=7+   lcdc=0xC1  pc=0x01C6 (stable) writes finished, loop settled
```

Real CGB hardware's boot ROM leaves the LCD **on** (`LCDC=$91`) and
every background palette color **white** before handing control to the
cartridge - pandocs' own `Power_Up_Sequence.md`, already correctly
modeled in this emulator's own `src/cpu.c` (`gb_cpu_reset()`, grounded
there with the identical citation - this project had already gotten
the real hardware behavior right, it just hadn't connected that fact
to this specific finding). The LCD is therefore live from the moment
control passes to the game - never explicitly turned off - and
`title_screen()` (`prism/src/title.c`), like every earlier milestone's
own initial `gems.c`/`hud.c`/`board.c` setup before it, wrote directly
to VRAM/tilemap/palettes without disabling the display first. Real
hardware would tear exactly the same way under identical code - a
well-known GB homebrew gotcha (real games disable the LCD, or wait for
VBlank, before bulk VRAM writes), not a timing defect in this
emulator's PPU model.

**Fix**: added `wait_vbl_done();DISPLAY_OFF;` as the first two
statements of `title_screen()`, before any of its setup writes - the
exact same real-hardware-safe pattern that function already used at
its own *end*, for the title→game transition, just applied to its
*start* too. Verified directly: frames 1-3 are now plain white (LCD
correctly off during setup) and frame 4 onward is the complete, untorn
title screen immediately - no partial/torn frame at any captured frame
count, confirmed by rendering and reading back every early frame, not
assumed from the mechanism alone.

A genuinely pleasant, unplanned result found while re-verifying:
turning the LCD off only pauses `frame_ready` (and therefore vblank
*frame numbering*) for the brief span `title_screen()`'s own setup
takes - once its own `DISPLAY_ON` fires, frame counting resumes
completely normally. Concretely, `input_script_m6b.txt`'s existing
scripted events (Start at frame 10, the same moves every script since
Milestone 5 has used) needed **no changes at all**, and the resulting
`.ppm`/`.sav` outputs from the exact same scripted run are still
byte-*identical* to the pre-fix `reference_m6b.ppm`/`reference_m6c.sav`/
`reference_m6c_title.ppm` - only `reference_m6c_sfx.wav` needed
re-locking, for the same reason Milestone 6c's own `highscore_init()`
addition once did (the added `wait_vbl_done()` shifts every subsequent
audio event's exact real-time/sample position by a small, fixed
amount, without shifting which frame number anything lands on).
Zero regressions: the full existing suite (unit tests, Mooneye 80/83,
`dmg-acid2` 100%, `cgb-acid2` 100%, `2048-gb`, `droneboy`,
`tobutobugirl`, savestate round-trip, all three RGBDS ROMs) stayed
green throughout - this change touches only `prism/src/title.c`,
nothing in the emulator core.

**Milestone 7 (this pass): done.** Animated gem swap - user-directed:
"the two pieces that switch [should be] animated instead of just
switching... you can see that they're switching places before the
[matched] tripple disappears." Previously `board_try_swap()`
(`prism/src/board.c`) mutated the grid and redrew instantly - a
matching swap jumped straight to the already-resolved (cleared/
collapsed/refilled) board with no transition, and a non-matching swap
just snapped back with only a sound effect for feedback. Asked the user
whether a reverted (non-matching) swap should also visibly slide back
apart or just snap back as before; they chose to animate both
directions, so every swap attempt gets consistent visual feedback.

New `prism/src/swapanim.c`/`swapanim.h`: `swapanim_play()` blanks the
two involved cells' background tiles, then slides two 16x16 sprite
gems across each other over 8 frames (2px/frame - an exact divisor of
the 16px cell pitch), reusing `gems.h`'s own diamond-quadrant bitmap
and palettes rather than authoring new art - loaded once more into
sprite-tile/OBJ-palette address space (confirmed via `gb/gb.h`'s own
`set_bkg_data()` doc note that sprite and background tiles only share
memory at IDs 128-255, well above every ID in use here). The sprite
icon itself reuses a single quadrant tile (`gem_tiles[0]`) mirrored via
`S_FLIPX`/`S_FLIPY` for all four corners - the same one-tile-plus-flips
technique `cursor.c`'s corner brackets already used - rather than
loading all 4 of the background version's separately-pre-flipped
quadrant tiles; verified the two are pixel-identical by checking the
committed `gem_tiles` bytes are exact bit-reversals/row-reversals of
each other before relying on it, then confirmed visually.

`main.c`'s `handle_select()` now peeks both cells' gem types
(`board_peek()`, new in `board.c`/`board.h`, alongside un-`static`-ing
`board_redraw()`) before the swap, hides the cursor and selection
marker for the animation's duration (`cursor_hide()`/`cursor_show()`,
new in `cursor.c`/`cursor.h` - keeps the concurrently-visible sprite
count under the hardware's 10-per-scanline limit; `swapanim.c`'s own 8
gem sprites already use most of that budget on a same-row horizontal
swap), plays the forward slide, then calls the unchanged
`board_try_swap()`. On a match, its own existing internal redraw
handles everything. On a revert, `main.c` explicitly plays the slide
back apart and calls the newly-exposed `board_redraw()` itself, since
`board_try_swap()` has no reason to redraw when nothing in `grid[]`
actually changed.

Verified past the point of "it compiled": traced individual pixel
columns frame-by-frame across a real captured swap (not just eyeballed
thumbnails, which made a real 2px/frame motion look deceptively static
at a glance) and confirmed both gems migrate monotonically toward each
other's cell and land exactly swapped; confirmed the revert path's
apparent "double oscillation" in that same pixel trace was the correct
there-and-back shape of a single forward+backward animation, not a
double-trigger bug, by adding a temporary one-shot serial-port counter
in `handle_select()` (reverted after use, same "instrument, observe,
revert" discipline the PPU quirk investigation above already
established) that confirmed the swap branch fires exactly once per
attempt. A one-frame render tear observed while probing arbitrary frame
counts (mid-redraw, the same well-understood category of risk any bulk
VRAM write during live gameplay already carried, not a new regression -
`board_redraw()` already ran unguarded mid-gameplay on every prior
milestone's matching swap) resolved by the very next frame, confirmed
transient rather than persistent before picking the final reference
frame count.

New `prism/input_script_m7.txt` (replaces `input_script_m6b.txt`) adds
a deliberately non-matching leading swap - (0,0) and (1,0), Blue and
Purple on the deterministic board, found by inspecting a captured
frame rather than guessed - ahead of the same matching swap
`input_script_m6b.txt` used, so both the forward-only and forward-then-
back animation paths are exercised and locked into the regression
reference. Every frame number re-derived empirically (each blocking
`swapanim_play()` call inserts up to 16 real frames that didn't exist
before this milestone); confirmed stable from frame 195 onward before
locking in `reference_m7.ppm` (replaces `reference_m6b.ppm`) and
`reference_m7_sfx.wav` (replaces `reference_m6c_sfx.wav` - now capturing
4 real SFX events, not 2: select, revert, select, match). Explicitly
re-confirmed `reference_m6c.sav` and `reference_m6c_title.ppm` are
**unaffected** by re-running those exact checks rather than assuming a
purely-rendering change couldn't touch them - both still pass unchanged,
since final score/moves state and the separate title-screen-only
invocation are untouched by adding an animation. Zero regressions: the
full existing suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%,
`cgb-acid2` 100%, `2048-gb`, `droneboy`, `tobutobugirl`, savestate
round-trip, all three RGBDS ROMs) stayed green throughout - this change
touches only `prism/`, nothing in the emulator core.

**Milestone 8 (this pass): done.** Animated gem clear - user-directed:
the matched gems should show "some kind of animation... rotating
and/or exploding" before they disappear, instead of
`collapse_and_refill()`/`board_redraw()` (`prism/src/board.c`) jumping
straight from "just swapped" to "fully resolved" with no transition
for the cells that actually cleared. True tile *rotation* isn't
practical on GB hardware for this shape - no hardware rotation exists,
and Milestone 7 already established that the diamond icon's 4
quadrants are exact bit-reversals of each other, meaning the shape is
symmetric under 90° rotation and would just look identical "rotated."
Asked the user to confirm the practical GB-native alternative instead -
a brief white flash then a 2-stage shrink to nothing, using two new,
smaller pre-drawn diamond tiles - and whether to include the flash;
they confirmed flash + shrink.

Two new BG tiles (`prism/src/gems.c`/`gems.h`, tile IDs 36-37 - the
next free range after `title.c`'s digit tiles, 26-35), generated via
the exact same Manhattan-distance formula the original diamond used
(confirmed by reproducing the original's own bytes from the formula
before trusting it for new values) - `lit iff x+y >= K` within an 8x8
quadrant, raised from the original `K=8` to `K=10` (medium, ~60% linear
size) and `K=12` (small, ~30%). Only one quadrant tile per stage, not
four - reused for all four corners via the CGB background attribute
byte's own flip bits (`gb/hardware.h`'s `BKGF_XFLIP`/`BKGF_YFLIP`,
confirmed to exist and mirror the `S_FLIPX`/`S_FLIPY` sprite
equivalents `swapanim.c` already uses for the identical "one tile, four
flipped corners" technique - just applied to the background side this
time, which pandocs' own attribute byte layout supports too). One new
CGB BG palette, `FLASH_PALETTE` (index 6 - `gems.c` owns 0-4, `title.c`
owns 5), a bright near-white, applied as an attribute-only change
against the *existing* full-size quadrant tiles for the one-frame
flash stage - needing no new tile data of its own at all.

New `board.c` static `play_clear_animation()`, called from
`board_try_swap()`'s cascade loop right before `collapse_and_refill()`
each pass, using the same `to_clear` grid that call already receives
and reading each matched cell's real gem color from `grid[][]` before
that call mutates it. Writes only the matched cells' own 2x2 tile
blocks (not a full redraw) through 3 stages - flash (2 frames), medium
shrink (4 frames), small shrink (4 frames) - then lets the existing
`collapse_and_refill()`+redraw naturally take it the rest of the way to
blank/refilled. `board_redraw()` moved from after the whole cascade
loop to inside it (once per pass) - a real correctness fix, not just
tidying: without it, a second cascade pass's own clear animation would
render against stale, pre-gravity tile art, since nothing would have
synced the screen to the first pass's `collapse_and_refill()` yet.

Verified by tracing actual captured frames (zoomed crops through the
match event), not just reasoning about the tile math: frame ~150 shows
the flash (matched cells uniformly bright white), ~154 the medium
shrink, ~160 the small shrink, matching the coded stage order and
timing exactly.

**A genuinely pleasant surprise found while re-verifying, not assumed**:
because `play_clear_animation()` only touches the *matched* cells'
tiles and the final `board_redraw()` still repaints the *entire* grid
identically to before once the cascade fully resolves, the actual
committed `prism/reference_m7.ppm` needed **no changes at all** - a
frame captured at the same, already-generously-margined frame count
(200) is still byte-*identical*, confirmed by direct `cmp`, not
inferred. Same for `reference_m6c.sav`/`reference_m6c_title.ppm`
(re-checked, not assumed). Only `reference_m7_sfx.wav` needed
re-locking (in place, same filename - not renamed to a new milestone
number, since nothing else changed), for the same by-now-familiar
reason: `sfx_play_match()` still fires right after `board_try_swap()`
returns, which now happens ~10 frames later than before (the new
animation's own duration), shifting that one event's exact real-time/
sample position without moving any vblank frame number a script event
depends on. A small, accepted feedback-timing side effect, not asked
about by the user and not worth new cross-module plumbing to avoid -
`board.c` stays free of any `sfx.c` dependency, preserving its existing
"pure grid mechanics" layering. Zero regressions: the full existing
suite (unit tests, Mooneye 80/83, `dmg-acid2` 100%, `cgb-acid2` 100%,
`2048-gb`, `droneboy`, `tobutobugirl`, savestate round-trip, all three
RGBDS ROMs) stayed green throughout - this change touches only
`prism/`, nothing in the emulator core.

**Next**: Milestone 6 (stretch/polish - sound effects, a real title
screen, SRAM high-score persistence), Milestone 7 (animated gem swap),
and Milestone 8 (animated gem clear) are all complete, and the PPU
timing finding is resolved. User-directed from here: give the SDL front end's
write-through-on-every-cart-RAM-write option a look (a natural, easy
enhancement flagged but deliberately not built during Milestone 6c),
resolve the GPL-license question to add a real homebrew regression
game, or build real networking/link infrastructure (would unlock both
real IR communication and real link-cable multiplayer at once, since
they're the same underlying gap).

**Phase 11 (a second original homebrew game - "Wayfarer"): scoped,
Milestone 1 (single-room movement) complete.** User-directed: "a basic
top-view game like the original Zelda." Confirmed as a genuinely
separate project from `prism/` (not an extension of it - a different
genre, match-3 vs. top-down action-adventure), CGB-only (same proven
toolchain/palette pipeline `prism/` already established), first
milestone scoped to the smallest real slice: a player sprite walking
around one static room with wall collision, no room transitions,
combat, or items yet.

**Toolchain reuse, not duplication**: `wayfarer/Makefile`'s
`GBDK_HOME` defaults to `../prism/toolchain/gbdk` - the same GBDK-2020
install `prism/` already needs - rather than fetching/extracting a
second copy of the same multi-hundred-MB release archive. A real,
deliberate decision, not an oversight; overridable if an independent
install is ever wanted. New top-level `wayfarer/` subproject, same
shape as `prism/` (own `Makefile`, own `README.md`). Root `Makefile`
gained one opt-in passthrough target, `gameboy-wayfarer-build` - same
"opt-in, external dependency, never part of plain `make`/`make
gameboy-test`" treatment every other GBDK/RGBDS/SDL target already
gets.

**Milestone roadmap** (whole-game scope): (1) toolchain bring-up +
single-room movement - this pass; (2) room transitions - multiple
rooms wired into a small overworld grid, screen-edge triggers; (3)
combat - a sword attack, one simple enemy type, hit detection, health;
(4) items/inventory + HUD; (5) stretch, later - a locked-door-and-key
puzzle room, sound effects, a title screen, SRAM save (reusing
`prism/`'s own now-proven `gb_cart_load_ram_file()`/`highscore.c`
pattern), a win condition.

**Milestone 1 (this pass): done.** `wayfarer/src/room.c` draws a
single static, fully-bordered room - the whole visible 20x18-tile
(160x144px) screen, a 1-tile wall ring around the outside (a simple
beveled-stone tile: dark top/left edge, light bottom/right edge, solid
fill between - not hand-drawn, computed from that rule and converted to
GB 2bpp planar tile bytes by a one-off local script, same discipline as
every tile asset in `prism/`) and open floor everywhere inside.
`wayfarer/src/player.c` draws a 16x16 directional player sprite (4 OAM
sprites, `prism/src/cursor.c`'s own `position_sprites()` shape) with
three authored tile sets - down (front-facing), up (back-facing), and
one side profile reused mirrored via `S_FLIPX` for left vs. right (the
same "one tile, four flipped corners" reuse technique `prism/`'s
`cursor.c`/`swapanim.c` already established, applied here as "one
profile, two flipped facings" instead) - swapping automatically as the
D-pad is held.

Movement is continuous (1px/frame, not grid-snapped) and
held-responsive (reads `joypad()`'s live state every frame, not
edge-triggered) - deliberately different from `prism/`'s own cursor,
which moves one grid cell per discrete press, since a top-down
action-adventure needs to read as real-time movement, not a menu
cursor. Collision is checked independently per axis against a plain
pixel-bounds query (`room_blocks()`) rather than general tile-by-tile
lookup - correct and sufficient since only the outer wall ring exists
this milestone - so the player can slide along a wall on one axis while
blocked on the other, rather than sticking outright.

Also carried forward directly rather than rediscovered: the real fix
for the "PPU rendering-catch-up quirk" this project's own `prism/`
investigation found and corrected (see this Status section's `prism/`
entries) - `wayfarer/src/main.c` disables the display
(`wait_vbl_done(); DISPLAY_OFF;`) before any of its own VRAM/tilemap/
palette setup writes, and only re-enables it (`SHOW_BKG; DISPLAY_ON;`)
once `room_init()`/`player_init()` have both finished, so this brand-new
project never has a chance to reintroduce the same real-hardware-shaped
torn-frame bug from scratch. Confirmed empirically, not just assumed
safe by carrying the pattern over: frames 1-3 (still mid-boot-ROM
white-screen state) differ from the final render, but frame 5 onward is
already byte-identical to frame 10 - no torn frame ever observed, in
contrast to `prism/title.c`'s own pre-fix behavior which did show one.

Verified via a new scripted `--input` sequence
(`wayfarer/input_script_m1.txt`) that holds each of the 4 directions in
turn - down, right, up, then left - for comfortably more frames than
the real distance needs, walking the player from the room's center into
every wall. Each leg's resulting frame was captured and visually
inspected (zoomed crops, not just thumbnails - the same discipline
`prism/`'s own Milestone 7 verification found necessary for a
few-pixel-per-frame change): confirmed the player stops flush against
the bottom wall, then the right wall, then the top wall, and finally
lands exactly in the top-left corner (flush against both the top and
left walls simultaneously, proving per-axis collision resolves
correctly even at a corner) - and that all three directional art sets
(down, right/side, and left/flipped-side) render correctly and
distinctly at each stop. Locked in as `wayfarer/reference_m1.ppm` only
after confirming frame stability (two captures 5 frames apart,
byte-identical). Full existing regression suite (unit tests, all
visual/game/savestate targets, all three RGBDS ROMs, Mooneye, and
`gameboy-prism-build` itself) stayed green throughout - this is a
wholly new, isolated subproject touching no emulator core code and no
existing `prism/` files.

**Milestone 2 (this pass): done.** User-directed: "let's build Milestone
2 - room transitions." Already scoped as an instant cut (not a scroll)
in Milestone 1's own wrap-up above, so no new design question there;
the real design work was the grid shape and how a room's own wall
layout should depend on it.

**Grid shape: 2x2** (`GRID_W=2`, `GRID_H=2`, `wayfarer/src/world.c`) -
the smallest grid that genuinely exercises both axes of transition
(east/west and north/south) in one milestone. Every room in a 2x2 grid
is a corner, so every room has exactly 2 open sides and 2 walled sides
- a real, easy-to-verify visual fingerprint per room (room (0,0):
east+south open; (1,0): west+south open; (0,1): east+north open; (1,1):
west+north open), not an artificial marker added just for testing.

**`wayfarer/src/room.c`/`room.h`**: `room_init()` now does only the
one-time tile/palette load (tile *data* is identical for every room -
only the tile *map* changes). New `room_draw(has_north, has_south, has_east, has_west)`
builds that map: a tile is wall iff it sits on a *closed* side's edge
column/row; a corner tile (on two edges at once) is wall unless *both*
adjoining sides are open, so a fully-open corner reads as a real,
walkable opening rather than a floating single-tile wall stub, and
stores the 4 flags for `room_blocks()`. `room_blocks()` changed from a
single fixed bound to a per-side one: a closed side keeps exactly
Milestone 1's bound (`ROOM_MIN_X`/`ROOM_MAX_X`/etc.); an open side
relaxes that out to the *absolute* screen edge a 16x16 sprite's
top-left corner can reach (`ABS_MIN_X=0`/`ABS_MAX_X=144`/etc.) rather
than removing the bound entirely - still blocks going *past* the true
edge (which would wrap a `uint8_t` position), leaving the exact edge
pixel as a clean hand-off point for `world.c`'s transition check to
catch before another frame's movement could underflow. With all 4
sides closed this is byte-for-byte Milestone 1's original behavior -
confirmed as a real regression check (room (1,0)'s own closed north
side, see Verification below), not just reasoned about.

**`wayfarer/src/player.c`/`player.h`** gained `player_get_x()`/
`player_get_y()` (read-only accessors, same shape as the sibling
`prism/`'s `cursor.c` own `cursor_get_x()`/`get_y()`) and
`player_set_position(x, y)` (repositions the sprite immediately via the
existing file-local `position_player()`, so a post-transition frame
never shows a stale position) - `player_update()`'s own movement/
collision logic is otherwise untouched.

**New `wayfarer/src/world.c`/`world.h`** owns the grid position
(`room_x`, `room_y`) and the transition check itself.
`world_update(joy)` calls `player_update(joy)` first (unchanged), then
checks - in a fixed west/east/north/south order, the same "fixed
priority, documented" tie-break `player.c`'s own diagonal-facing logic
already uses for the rare exact-corner case - whether the player is
sitting exactly on an *open* absolute edge. On a match: the same
real-hardware-safe screen-off pattern `main.c`'s own boot sequence
already uses (`wait_vbl_done(); DISPLAY_OFF;` ... `SHOW_BKG; DISPLAY_ON;`),
reused here because a full room-tile-map rewrite mid-gameplay is the
same "bulk VRAM write while the LCD is live" risk category as any
other setup write in this project - worth actually guarding here,
unlike the sibling `prism/` project's own per-*cell* `board_redraw()`,
which accepts a rare single-frame tear as a known, documented tradeoff
for a much smaller write. Then updates `room_x`/`room_y`, redraws via
`room_draw()`, and calls `player_set_position()` to place the player
just inside the new room's opposite edge (e.g. exiting west sets the
new room's entry `x = ROOM_MAX_X`, `y` unchanged - the whole edge is
open, so no realignment is needed).

**Verified** via a new scripted `--input` sequence
(`wayfarer/input_script_m2.txt`, replacing `input_script_m1.txt`) that
holds RIGHT to cross room (0,0)'s open east edge into room (1,0)
(confirmed by eye: the wall-opening pattern changes from east+south-open
to west+south-open, exactly the expected fingerprint), then holds UP to
confirm room (1,0)'s own *closed* north side still blocks movement
(the player stops flush against the top wall, same as Milestone 1's
own collision behavior, now proven in a non-origin room), then holds
DOWN to cross (1,0)'s open south edge into room (1,1) (west+north-open
fingerprint) - a second, chained transition. Every frame number
re-derived empirically, not guessed. Locked in as
`wayfarer/reference_m2.ppm` only after confirming frame stability;
`reference_m1.ppm`/`input_script_m1.txt` deleted once superseded (this
project's own "supersede, don't accumulate" convention, already
established by the sibling `prism/` project's history). Full existing
regression suite (unit tests, all visual/game/savestate targets, all
three RGBDS ROMs, Mooneye, `gameboy-prism-build`) stayed green
throughout - this change touches only `wayfarer/`.

**Milestone 3 (this pass): done.** User-directed: "let's build Milestone
2 - room transitions" (prior pass), then "do next step" for this one.
Confirmed scope up front: the sword defeats the enemy this pass, but
the enemy cannot hurt the player yet - player health/damage/game-over
is real, separate scope, deliberately deferred to its own later
milestone rather than folded in here, same "one provable slice"
discipline Milestones 1-2 both used. Enemy movement: a simple
deterministic back-and-forth patrol, not stationary - both confirmed
via `AskUserQuestion` rather than assumed.

**`wayfarer/src/sword.c`/`sword.h`** (new): a one-shot swing,
edge-triggered by `A` (`SWORD_FRAMES=12`, ~200ms), with no movement
lock - the player can keep walking mid-swing, a real, deliberate scope
decision (simplest correct behavior, not worth extra state to
prevent). Two 8x8 sprite tiles - a vertical bar (up/down) and a
horizontal bar (left/right); a straight bar is already symmetric under
its own axis's flip, so unlike `player.c`'s own directional art no
flip logic is even needed. The swing's 8x8 hitbox is recomputed from
the player's *current* position/facing every active frame (one
tile-width beyond whichever edge of the 16x16 player the facing points
at, centered on the other axis) and cached in the same static fields
`sword_get_x()`/`get_y()` return - a small, deliberate refactor during
implementation (an earlier draft recomputed the hitbox a second time
inside the getters via a duplicated `switch`, a real duplication risk
where the drawn sprite and the hit-test position could silently drift
apart; storing one copy removes the risk outright rather than trusting
the two copies to stay in sync).

**`wayfarer/src/enemy.c`/`enemy.h`** (new): one 8x8 round blob
(circular-distance generated, same per-pixel-grid script technique as
every tile asset in this project), deliberately smaller/simpler than
the 16x16 player - a weaker first enemy, not a scope shortfall.
Patrols a fixed horizontal range at a fixed `y`, 1px/frame, reversing
at each bound - fully deterministic. Belongs to room (0,0) only (a
single instance is enough for "one enemy type," no per-room entity
list needed yet - genuinely future scope, not built speculatively).
`enemy_try_hit()` is a plain AABB-overlap check against its own 8x8
box, owns its own `alive` flag, and is the only thing that mutates it.

**`wayfarer/src/world.c`** wires the two together without either
module knowing about the other (mirrors how the sibling `prism/`
project's own `main.c` wires `board.c`+`swapanim.c`+`sfx.c`): adds
`sword_init()`/`enemy_init()` to `world_init()`; `world_update()` gains
its own edge-detected `A` press (`prev_joy`, the same
`pressed = joy & ~prev_joy` pattern `prism/`'s `cursor.c`/`main.c`
already established, now needed here for the first time since
Milestones 1-2 were purely level-triggered movement) forwarded to
`sword_update()`, and - only while the current room is (0,0) - calls
`enemy_update()` and, if `sword_is_active()`, `enemy_try_hit()`. On the
transition branch specifically, leaving room (0,0) for anywhere else
calls `enemy_hide()` right there, since sprites persist across a
BG-only room redraw unless explicitly moved - confirmed for real (not
just reasoned about): a scripted room-exit while the enemy was still
alive showed it correctly vanish rather than follow into room (1,0).

**Verified** via a new scripted `--input` sequence
(`wayfarer/input_script_m3.txt`, replacing `input_script_m2.txt`) that
starts with a real negative-case check - tapping `A` immediately
(facing down, nowhere near the enemy) and confirming, several frames
later, the enemy is provably unaffected - before holding RIGHT to
approach and landing a real hit, confirmed by tracing individual
frames around the swing (the enemy vanishes within a couple of frames
of the second `A` press and never reappears). The exact tap frame for
a landed hit needed real empirical search, not just a plausible guess:
the enemy's continuous patrol means the same approach timing that
worked at one absolute frame count missed when an earlier unrelated
input (the negative-case tap) shifted every later frame number by a
few frames - resolved by scanning candidate tap frames and visually
confirming the actual hit, the same "derive empirically, don't assume
timing transfers" discipline every prior milestone's own scripted
verification has needed at least once. Locked in as
`wayfarer/reference_m3.ppm` only after confirming frame stability;
`reference_m2.ppm`/`input_script_m2.txt` deleted once superseded. Full
existing regression suite (unit tests, all visual/game/savestate
targets, all three RGBDS ROMs, Mooneye, `gameboy-prism-build`) stayed
green throughout - this change touches only `wayfarer/`.

**Milestone 4 (this pass): done.** User-directed: "do next" after
Milestone 3. Roadmap Milestone 4 is "items/inventory + HUD," but a full
inventory system has no real use yet - no locked doors exist for a key
to open, that's a later milestone's own stretch scope - so, confirmed
with the user, this pass is the honestly-scoped slice: player health
(hearts), a heart-icon HUD, the enemy can now damage the player on
contact, and one heart pickup that heals - a real, immediately-useful
item, not a placeholder. On hearts reaching 0: confirmed auto-respawn
immediately, no button press needed.

**`wayfarer/src/room.h`** gained `ROOM_CENTER_X`/`ROOM_CENTER_Y`
(derived from the existing `ROOM_MIN/MAX_X/Y`) - one source of truth
for "where the player starts/respawns," shared by `player.c`'s own
`player_init()` (previously its own inline arithmetic) and `world.c`'s
new respawn target.

**`wayfarer/src/player.c`/`player.h`** gained `MAX_HEARTS=3`,
`INVINCIBILITY_FRAMES=60` (~1s - long enough that a single enemy pass,
which takes only ~24 frames to fully cross a stationary 16px-wide
player at 1px/frame, can't cause a second deduction from the same
graze), `player_damage()`/`player_heal_full()`/`player_get_hearts()`.
No knockback, no invincibility-flicker visual feedback this pass - real,
deliberate scope decisions, simplest correct behavior, stated plainly.

**New `wayfarer/src/heart_hud.c`/`heart_hud.h`**: a 3-sprite,
fixed-screen-position HUD - deliberately sprites, not BG tiles, so
Milestones 1-2's already-locked-in room/wall geometry needs zero
changes; sprites simply draw on top of whatever room tile is
underneath. One hand-authored 8x8 heart-shape tile (a heart silhouette
isn't formulaic like the diamond/circle shapes every other tile asset
here used, so drawn directly as a per-pixel grid, then verified by
rendering before being committed). Two OBJ palettes share that one
tile - full (bright red) and empty (dim gray) - the same "one tile,
palette-swapped" technique the sibling `prism/` project's own
`board.c` clear-animation flash uses, rather than a second bitmap.

**New `wayfarer/src/pickup.c`/`pickup.h`**: one heart pickup, reusing
`heart_hud.h`'s own exported tile ID and "full" palette directly - zero
art duplication, same tile, a different sprite slot/position. Belongs
to room (1,1) only, fixed at that room's own center
(`ROOM_CENTER_X`/`Y`). Unlike the enemy's patrol, a stationary pickup
has no per-frame update to fall back on for re-showing itself after
being hidden, so `world.c` calls `pickup_show()` explicitly on
entering its room, not just `pickup_hide()` on leaving.

**`wayfarer/src/world.c`** refactored its existing inline
transition-redraw block into a new static `go_to_room()` - a real,
non-cosmetic factor-out: the on-death respawn path needs the exact same
`wait_vbl_done(); DISPLAY_OFF; ...; SHOW_BKG; DISPLAY_ON;` sequence
(now also handling `pickup_show()`/`hide()` alongside the existing
`enemy_hide()`), and duplicating that block between two call sites
would be exactly the kind of drift risk this project's own
"guard real-hardware-safe timing carefully" discipline argues against.
`enemy.h` gained `enemy_get_x()`/`get_y()`/`is_alive()` read-only
accessors for a *second*, separate AABB check (player-vs-enemy contact
damage) alongside the existing sword-vs-enemy one already living inside
`enemy_try_hit()` (which also mutates state, so isn't reused for this
read-only check). `world_update()` now also calls `heart_hud_update()`
unconditionally every frame, checks player-vs-enemy overlap while in
the enemy's room (`player_damage(1)` on contact, a no-op if still
invincible), triggers `go_to_room()`-based respawn at 0 hearts, and
checks `pickup_try_collect()` while in the pickup's room.

**Verified** via a new scripted `--input` sequence
(`wayfarer/input_script_m4.txt`, replacing `input_script_m3.txt`):
standing in the enemy's patrol lane (no attack) costs exactly one heart
- confirmed via the HUD (the third heart's OBJ palette swaps to dim
gray) - and, by sampling several frames across the enemy's entire pass
through the stationary player, confirmed the *same* pass doesn't cost
a second heart (invincibility holding). Continuing across two chained
room transitions (Milestone 2's own verified east-then-south path) into
room (1,1) and walking onto the pickup visibly heals back to full (all
three hearts red again) and the pickup sprite disappears for good.
Respawn-on-zero-hearts didn't need a separate throwaway build (the
plan's own fallback, mirroring `prism/`'s Milestone 5 precedent) - the
same patrol-lane scenario, left running long enough, produced a real
third hit and a real respawn (hearts refilled, player back at
`ROOM_CENTER_X/Y`), observed directly frame-by-frame. Locked in as
`wayfarer/reference_m4.ppm` only after confirming frame stability;
`reference_m3.ppm`/`input_script_m3.txt` deleted once superseded. Full
existing regression suite (unit tests, all visual/game/savestate
targets, all three RGBDS ROMs, Mooneye, `gameboy-prism-build`) stayed
green throughout - this change touches only `wayfarer/`.

**Milestone 5 (this pass): done - locked door + key.** Roadmap
Milestone 5 bundles five independent stretch items (locked-door-and-key
puzzle room, sound effects, a title screen, SRAM save, a win
condition); asked the user which to build first - locked door + key,
the first real use for an actual inventory item.

**A locked door only means something if it actually gates progress**:
the plain 2x2 room grid Milestones 1-4 built is a full 4-room cycle
(every room connects to both its grid neighbors), so locking just one
of the four edges would leave the other three as a free bypass -
decorative, not a real puzzle. This pass also permanently severs one
*other* edge - `(0,0)`'s south side, always closed now, not key-gated,
just different level geometry - turning the map into a genuine
dead-end chain: `(0,0)` [start, `enemy.c`] → east → `(1,0)` [new: the
key] → south → `(1,1)` [`pickup.c`'s heart] → **locked door** → `(0,1)`
[new: otherwise completely unreachable]. `(0,1)`'s own north side is
severed to match, so before the key is collected that room has *no*
open sides at all - correctly, intentionally unreachable, not a bug.

**`wayfarer/src/room.c`/`room.h`** gained a third BG tile -
`DOOR_TILE_ID`, a hand-drawn gold fill with a dark vertical center
seam (suggesting a pair of doors), its own second BG palette
(`DOOR_PALETTE`) - distinct enough from the plain beveled-stone wall
tile to read as "something that opens," not just more wall.
`room_draw()` gained one more, purely cosmetic parameter (`door_side`,
new `DOOR_NONE/NORTH/SOUTH/EAST/WEST` constants) that only decides
*which tile* renders on an already-closed side - it never touches
`has_north/south/east/west`, which are still the sole thing driving
`room_blocks()`. This is the real design payoff of Milestone 2's own
per-side bound design: a locked door needed **zero changes to
collision logic at all** - it's closed because that side's `has_*`
flag is 0 (same as any other wall), and once the key flips that flag
to 1, it's already passable for free, reusing already-correct,
already-tested code rather than adding a parallel "is this side
locked" collision check that could drift out of sync with it.

**`wayfarer/src/world.c`** factored `has_north/south/east/west`'s
computation - previously duplicated between `draw_current_room()` and
`world_update()`'s own transition check, a real pre-existing
duplication - into one new static `compute_sides()`, now also home to
both the permanent-sever and key-gated-lock overrides described above
(`(1,1)`'s west and `(0,1)`'s east read `key_is_collected()` directly
instead of their plain topological default). New `door_side_for()`
feeds `room_draw()`'s new parameter. `go_to_room()` gained
`key_show()`/`key_hide()` calls symmetric to the existing pickup ones.

**New `wayfarer/src/key.c`/`key.h`**: a near-twin of `pickup.c` (same
`init()`/`show()`/`hide()`/`try_collect()` shape, fixed at
`ROOM_CENTER_X`/`Y` in room `(1,0)`) - deliberately *not* generalized
into a shared "collectible item" base with `pickup.c`: the two behave
differently enough (heal vs. flip a door's lock) and there are only
two of them total, so a shared abstraction would be premature
machinery for two call sites, not saved duplication. One new
hand-authored 8x8 key-shape tile (ring, shaft, one tooth - not
formulaic like a diamond/circle, drawn directly and verified by
rendering, same discipline `heart_hud.c`'s heart used), one new OBJ
palette (gold). Collecting the key has no immediate visible effect
beyond the door itself the next time it's drawn - no key HUD icon this
pass, a real, deliberate scope decision, stated plainly rather than
silently missing (the same way a real Zelda key pickup doesn't
announce itself beyond the door it opens).

**Verified** via a new scripted `--input` sequence
(`wayfarer/input_script_m5.txt`, replacing `input_script_m4.txt`):
holding RIGHT through room `(1,0)` collects the key (confirmed by the
key sprite disappearing and staying gone); a separate throwaway probe
(not part of the committed script) confirmed the locked door in room
`(1,1)` renders with the door texture and genuinely blocks movement -
the player stops flush against it, no transition - before the key is
collected. Continuing the real script south into room `(1,1)` and then
west into the now-unlocked door successfully transitions into room
`(0,1)`, confirmed by its own distinct wall fingerprint (only east
open, back toward the door) - a room reachable no other way. Locked in
as `wayfarer/reference_m5.ppm` only after confirming frame stability;
`reference_m4.ppm`/`input_script_m4.txt` deleted once superseded (room
`(0,0)`'s own boot-frame wall pattern changed regardless of this
feature, since its south side is now permanently walled - a real,
expected visual change, not a regression). Full existing regression
suite (unit tests, all visual/game/savestate targets, all three RGBDS
ROMs, Mooneye, `gameboy-prism-build`) stayed green throughout - this
change touches only `wayfarer/`.

**Milestone 6 (this pass): done - win condition.** Four independent
stretch items remained (sound effects, a title screen, SRAM save, a
win condition); asked the user which to build first - win condition:
defeat the enemy *and* reach room `(0,1)` to end the session with a
real win state. Recommended since it needed no new infrastructure
category (no APU work, no SRAM) and now had a genuine payoff to
display, Milestone 5 having made room `(0,1)` mean something.

**New `wayfarer/src/win.c`/`win.h`**: a small, self-contained text
overlay reusing the sibling `prism/` project's own `title.c` technique
(a hand-authored 5x7 block-letter tileset, left-aligned within the
8-bit row for consistent right-side spacing, same public-domain
block-font convention) - but only the 3 letters "WIN" actually needs,
not a general alphabet. `win_play()`: this project's own
real-hardware-safe screen-off pattern
(`wait_vbl_done(); DISPLAY_OFF;`), then `HIDE_SPRITES` (one hardware
macro, confirmed present in the vendored GBDK headers, that turns off
the whole OBJ layer at once - `win.c` never needs to know about or
call into `player.c`/`sword.c`/`enemy.c`/`heart_hud.c`/`pickup.c`/
`key.c`'s own sprite state individually, a real simplification), then
blanks the whole 20x18 tile map by reusing `room.h`'s own
`FLOOR_TILE_ID` (newly exported from `room.c`, where it was previously
a private `#define`) under a new gold `WIN_PALETTE` - the same "one
tile, palette swap" trick `heart_hud.c`'s full/empty hearts and
`prism/`'s own clear-animation flash both already use, so no second
blank-tile bitmap was needed - and draws "WIN" centered in that same
palette.

**`wayfarer/src/world.c`** gained a static `won` flag and, as
`world_update()`'s very first line, `if (won) return;` - a one-shot,
terminal freeze, the simplest correct "the game is over" state,
matching this project's own "always fully resolve, no half-finished
states" style already used for Milestone 4's own auto-respawn. The win
check itself sits near the *end* of `world_update()`, after all other
per-frame logic: `if (!won && !enemy_is_alive() && room_x == WIN_ROOM_X && room_y == WIN_ROOM_Y)`
- checked unconditionally every frame rather than only at the specific
moment either condition changes, since either the room-entry
transition or the sword-kill earlier in that same function call could
be the *second* of the two conditions to become true.

**Verified** via a new scripted `--input` sequence
(`wayfarer/input_script_m6.txt`, replacing `input_script_m5.txt`) that
combines two earlier milestones' own already-verified timing back to
back: Milestone 3's sword-kill sequence (still valid unchanged, since
nothing about the enemy's deterministic patrol depends on anything
added since), then Milestone 5's own locked-door path - confirming the
win screen appears (clean "WIN" on a gold background, no sprites
visible) and stays frozen many frames later (byte-identical, not
reverting). **A real negative-case check** too, cheaper than scripting
a second full run: running the *prior*, still-on-disk Milestone 5
script (which reaches room `(0,1)` but never attacks the enemy)
against this milestone's actual build showed no win screen at all -
direct proof both conditions are genuinely required, not just the room
transition alone. Locked in as `wayfarer/reference_m6.ppm` only after
confirming frame stability; `reference_m5.ppm`/`input_script_m5.txt`
deleted once superseded. Full existing regression suite (unit tests,
all visual/game/savestate targets, all three RGBDS ROMs, Mooneye,
`gameboy-prism-build`) stayed green throughout - this change touches
only `wayfarer/`.

**Milestone 7 (this pass): done - sound effects.** User-directed.
Directly reuses the sibling `prism/` project's own `sfx.c` approach -
raw DMG/CGB sound-register pokes (this emulator's own APU already
fully implemented and verified, `src/apu.c`/`tests/test_apu.c`/
`test_roms/droneboy`), no higher-level GBDK sound API, no per-frame
service loop or tracker - grounded the same way (pandocs' own
period-frequency formula, computed offline, not hand-derived).

**New `wayfarer/src/sfx.c`/`sfx.h`**: five one-shot triggers, each a
single write-and-restart to one channel's registers.
`sfx_play_swing()` (channel 1, short high C6 blip, period 0x783) and
`sfx_play_hit()` (channel 1, rising sweep from a G4 base, period
0x6B2) are near-identical in shape to `prism/`'s own
`sfx_play_select()`/`sfx_play_match()` - same notes, even, since both
projects share the same "short blip for an action, rising sweep for a
success" vocabulary. `sfx_play_pickup()` (channel 1, a distinct E6
blip, period 0x79D) is shared by both the heart pickup and the key - a
real, deliberate scope decision: both are "you got something," and a
second, extremely similar trigger wasn't worth building just to tell
them apart audibly. `sfx_play_damage()` (channel 4, noise) is
higher-pitched and much shorter than `prism/`'s own
`sfx_play_gameover()` (shift 5 vs. its 11, length counter left *on*
for a firm ~74ms burst rather than a long dramatic fade) - a routine
"you got hit," not a session-ending event. `sfx_play_win()` reuses
`sfx_play_hit()`'s exact sweep rate and base note, just held longer
(~203ms vs. ~148ms) - a triumphant variant rather than a fully
distinct sound, verified safe against the sweep-overflow ceiling the
same way `prism/`'s own `sfx_play_match()` comment already worked out
(the longer note's rise reaches ~2052 right around its own natural
203ms end, not a premature cutoff).

**`wayfarer/src/world.c`** gained every `sfx_play_*()` call site -
matching `prism/main.c`'s own convention exactly, none of
`sword.c`/`enemy.c`/`pickup.c`/`key.c`/`win.c` themselves ever call
into `sfx.c`, staying unaware audio exists at all (the same layering
`prism/`'s `board.c`/`swapanim.c` already keep relative to its own
`sfx.c`). The swing trigger needed one small new piece of state:
capturing `sword_is_active()` *before* calling `sword_update()`, so a
genuinely new swing (as opposed to an ignored repeat press mid-swing)
can be told apart. `wayfarer/src/player.h`/`player.c`'s
`player_damage()` changed from `void` to `uint8_t` (1 if damage was
actually applied, 0 if it was a no-op due to invincibility) - a small,
natural extension matching this project's own established "return
whether something happened" convention
(`enemy_try_hit()`/`pickup_try_collect()`/`key_try_collect()` all
already do this) - so the damage sound plays only on a real hit, not
every frame of a graze already blocked by invincibility.
`wayfarer/src/main.c` gained one line, `sfx_init()`, alongside
`world_init()`.

**Verified** the same way `prism/`'s own Milestone 6a was: reused
`wayfarer/input_script_m6.txt` **unchanged** (confirmed byte-identical
rendered frame via direct `cmp` against `reference_m6.ppm` - sound is
audio-only, provably can't affect it) with a new `--wav` capture
(`--seconds 9`, comfortably past the script's last event at frame 420),
verified programmatically (Python's `wave` module, the same
"sample and confirm objectively" discipline every prior audio/pixel
verification in this project has used) rather than eyeballed: silence
before the first real event, then exactly 4 clean, correctly-timed,
distinct events - the early negative-case swing (~0.22-0.30s), the
real swing+hit pair when the enemy is defeated (~0.88-1.16s, the two
close enough together to show as one continuous run), one incidental
contact-damage hit during the approach (~2.72-2.82s, matching
Milestone 6's own finding that hearts were already at 2/3 by the time
the enemy died in this exact script), and the win trigger
(~6.64-6.84s). The heart-pickup call site specifically isn't reachable
by this script (it never walks over the heart in room `(1,1)`) -
verified once via a separate, uncommitted probe script instead (walked
onto the heart directly, confirmed a second `sfx_play_pickup()`-shaped
event fired), the same asymmetric-verification precedent `prism/`'s
own M6a used for its game-over sound. Locked in as
`wayfarer/reference_m7_sfx.wav` (no existing wayfarer WAV reference to
supersede). Root `Makefile`'s `gameboy-wayfarer-build` recipe extended
with a `--wav`+`cmp` step (new `WAYFARER_WAV_OUT`/`WAYFARER_WAV_REF`
variables), alongside the existing `--ppm` step. Full existing
regression suite (unit tests, all visual/game/savestate targets, all
three RGBDS ROMs, Mooneye, `gameboy-prism-build`) stayed green
throughout - this change touches only `wayfarer/`.

**Milestone 8 (this pass): done - title screen.** User-directed,
between the two remaining stretch items (title screen, SRAM save).
Directly reuses the sibling `prism/` project's own `title.c` technique
- a hand-authored 5x7 block-letter tileset, real-hardware-safe
LCD-disable timing at both the start and end of the screen - a
technique this project's own Milestone 6 (`win.c`) already reused once
for its "WIN" screen, so this is the third application of the same
proven pattern, not a new one.

**New `wayfarer/src/title.c`/`title.h`**: its own, non-overlapping BG
tile range (`TITLE_TILE_BASE = 6` - `room.c` owns 0-2, `win.c` owns
3-5), matching `prism/`'s own precedent of giving `title.c` a fresh
range rather than reusing `hud.c`'s, even though they're temporally
exclusive, for simplicity/safety. Only the 9 letters actually needed
for "WAYFARER"/"PRESS START" (`W,A,Y,F,R,E,P,S,T`) plus a blank -
independently derived but confirmed to match `prism/title.c`'s own
`P/R/E/S/T/A` bytes and this project's own `win.c`'s `W` bytes exactly
(the same standard block-font convention, not copied art). Its own
private blank tile, *not* reusing `room.h`'s `FLOOR_TILE_ID` the way
`win.c` safely does - `title_screen()` runs *before* `room_init()`
ever loads that tile's real data, so reusing it here would render
undefined VRAM content; `prism/title.c` has this exact same constraint
and handles it the same way. `title_screen()`'s own structure mirrors
`prism/title.c` line for line: `wait_vbl_done(); DISPLAY_OFF;` first
(the boot ROM leaves the LCD on with white palettes), draw, `SHOW_BKG;
DISPLAY_ON;`, an edge-triggered wait-for-`Start` loop, then
`wait_vbl_done(); DISPLAY_OFF;` again before returning so
`world_init()`'s own tile loading never lands on a live display.
`wayfarer/src/main.c` simplified: its own explicit boot-time
`wait_vbl_done(); DISPLAY_OFF;` block is now `title_screen()`'s
responsibility. Unlike `prism/`'s own Milestone 6b, no `prev_joy`-
seeding concern was found or needed - Wayfarer doesn't bind `Start` to
anything during gameplay, so a phantom edge from a still-held button
has no behavior to guard against here.

**A real, unavoidable consequence, not an oversight**: the title
screen is a genuine player-paced wait, so every existing scripted frame
number needed re-deriving - the same category of thing `prism/`'s own
Milestone 6b hit. **A real, worth-stating finding came out of this**:
probing confirmed the movement-duration legs (`RIGHT`/`DOWN`/`LEFT`
held for N frames) are shift-invariant - an N-frame hold always moves
the player N pixels, regardless of what absolute frame gameplay
actually started on, since it only depends on frames-since-input, not
frames-since-power-on. But the sword-kill's own precise hit-window
timing is *not* shift-invariant in the same simple way: a naive
uniform-shift guess (add a fixed delta to every old frame number)
predicted the kill would land at frame 61; scanning nearby candidate
frames instead found the real working hit at frame 56 - five frames
earlier. Had the uniform-shift assumption been trusted instead of
verified, the resulting script would have looked plausible (correctly
shaped, right button sequence) while being silently wrong (the enemy
would still have been alive at the scripted swing, and the whole
downstream win-condition chain would never trigger) - exactly the
failure mode this project's own "derive empirically, don't assume
timing transfers" discipline exists to catch, confirmed here rather
than just reasoned about.

**Verified** via a new scripted `--input` sequence
(`wayfarer/input_script_m8.txt`, replacing `input_script_m6.txt`) that
re-derives the complete path from a fresh title-screen boot: `START`
to dismiss, a negative-case sword swing, the real sword-kill (at the
empirically-confirmed frame 56), collecting the key in room `(1,0)`,
crossing into room `(1,1)`, and unlocking/crossing the door into room
`(0,1)` - confirming the Milestone 6 win screen still triggers
correctly end to end. A new `--wav` capture over the same script
confirmed the same 4 distinct, correctly-timed sound events Milestone
7 already established, just at their own newly-shifted sample
positions. Locked in as `wayfarer/reference_m8.ppm`/
`reference_m8_sfx.wav` only after confirming frame stability;
`reference_m6.ppm`/`reference_m7_sfx.wav`/`input_script_m6.txt`
deleted once superseded. Full existing regression suite (unit tests,
all visual/game/savestate targets, all three RGBDS ROMs, Mooneye,
`gameboy-prism-build`) stayed green throughout - this change touches
only `wayfarer/`.

**Next**: SRAM save (persisting key/pickup/enemy-defeated/win state
across power cycles, reusing this emulator's now-proven
`gb_cart_load_ram_file()` pattern from `prism/`'s own Milestone 6c) is
the one remaining stretch item, once user-directed.
