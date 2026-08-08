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

### Phase 7 (exploratory, not scoped)

- **Game Boy Color (CGB) support** - double-speed mode, the extra
  VRAM/WRAM banks, color palettes. A real hardware revision with its
  own documented differences, not guessed.
- **Save states / battery-backed cartridge RAM persistence.**
- **A real graphical front end.** The Game Boy's output is a pixel
  framebuffer, not text - the `cpm/gtk/` subproject's approach (spawn the
  real, unmodified core binary attached to a pty, let a `VteTerminal`
  widget do the interpretation) doesn't transfer directly, since there's
  no terminal escape-code stream to interpret. A GTK+Cairo (or SDL2)
  window blitting a pixel buffer is the likely shape, decided when this
  phase actually starts rather than now.

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

**Next**: three known, distinct, honestly-scoped-out gaps remain across
the committed Mooneye subset (61/65 overall) - none blocking, none
guessed at:

- `pop_timing.gb` (and the last unresolved assertion each in
  `timer/tima_write_reloading.gb`/`tma_write_reloading.gb`): needs real
  per-M-cycle timer precision - see the attempt-and-revert entry below
  for why this turned out to be a genuinely bigger change than the
  OAM-DMA-timing rewrite above, not a smaller version of the same
  pattern as first assumed.
- `mbc1/multicart_rom_8Mb.gb`: MBC1M's genuinely distinct addressing
  scheme, needs its own multicart-detection heuristic first.
- `acceptance/ppu/` (11) + `acceptance/oam_dma*` (6), the two Tier 2
  slices deferred since the original Tier 1 adoption - now that OAM DMA
  is real and timed, these are worth revisiting; the `oam_dma*` ones in
  particular may already pass, or come close, as a direct consequence
  of this rewrite, unverified since they were never part of the
  committed subset.

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
