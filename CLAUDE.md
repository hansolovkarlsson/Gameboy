# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A Game Boy emulator written in C - both the original DMG (the 1989
monochrome hardware) and the Game Boy Color (CGB).
Originally developed as a subproject inside a Z80/CP-M emulator repo,
then split out into this standalone repo (via `git subtree split`,
preserving its real commit history) once it became clear the two
shared no code at all - see `docs/GAMEBOY_ROADMAP.md`'s "Architecture
decision" section for the reasoning, and its Status section (near the
end) for the split itself. Comments throughout this codebase still cite
`cpm/...` paths from that sibling Z80/CP-M repo (a separate GitHub
repository now, not a directory here) where a design decision was
genuinely compared against or modeled on that project's own approach -
those citations are accurate as "this is the real prior art", just no
longer "elsewhere in this same repo".

Three docs live in `docs/`: `GAMEBOY_ROADMAP.md` (project status by
phase - what's done, what's next, and the honest reasoning/evidence
behind every fix, not just a checklist), `CPU_REFERENCE.md` (the SM83
instruction set, including how and where it genuinely differs from a
real Z80), and `HARDWARE_REFERENCE.md` (the DMG hardware spec this
emulator targets - memory map, cartridge/MBC banking, PPU, APU, timer,
joypad - each claim grounded against a real, cited pandocs page, not
guessed). This file (`CLAUDE.md`) covers build/run/test commands and
day-to-day conventions; read the roadmap for *why* something is
implemented the way it is.

## Build & Run

```
make                    # builds bin/gameboy (the core --ppm/--wav/--input driver)
make gameboy-test       # build + run every direct, ROM-independent unit test
make gameboy-visual-test    # dmg-acid2: render a frame, compare pixel-for-pixel
make gameboy-2048-test      # 2048-gb: scripted play, byte-exact frame regression
make gameboy-droneboy-test  # Droneboy: byte-exact audio regression
make gameboy-tobu-test      # Tobu Tobu Girl: byte-exact frame regression
make gameboy-savestate-test # real-ROM save/load round-trip, byte-exact
make gameboy-rgbds-test        # opt-in, needs `brew install rgbds`
make gameboy-rgbds-mbc3-test   # opt-in, needs `brew install rgbds`
make gameboy-sdl        # opt-in, needs SDL2 (`brew install sdl2`) - the real
                         # playable front end: video, keyboard input, live
                         # audio, and save states (F5/F9)
make clean               # remove object files and all built binaries
```

`bin/gameboy` takes a ROM path as argv[1]:

```
bin/gameboy test_roms/dmg-acid2/dmg-acid2.gb --ppm out.ppm --frames 2
```

See `bin/gameboy --help` for the full flag set (`--ppm`/`--wav`/
`--input`/`--load-state`/`--save-state`), and `README.md`'s "Playing"
section (or `bin/gameboy-sdl --help`) for the SDL front end's key
bindings.

Correctness is verified two ways, both grounded rather than
self-referential: **real ROMs** (`test_roms/` - open-source homebrew
games and PPU/APU/hardware test suites, each with its own committed
license and a `README.md` documenting exactly what it tests and any
bug it found) compared byte-for-byte against known-good captured output
where possible, and **direct unit tests** (`tests/`) for behavior a
whole-ROM comparison can't isolate cleanly (a specific hardware quirk,
a save-state field, a timer edge case). Every bug fix in this project's
history added a regression test of one of these two kinds alongside
the fix itself, not after the fact - see `docs/GAMEBOY_ROADMAP.md`'s
Status section for the specific real bugs each one caught.

`roms/` is real cartridge dumps - gitignored, **never committed, not
even to a private repo** (Nintendo's copyrighted work, unlike 1980s
CP/M software's much murkier situation - see `README.md`). `test_roms/`
is the opposite: open-source, explicitly-licensed test content, safe to
commit and already committed.

## Conventions

- Ground every technical claim in a real primary source (pandocs for
  Game Boy hardware, the actual upstream ROM/tool source for anything
  cited from it) - never guess at hardware behavior.
- Verify a ROM's license against its real, linked repository (not just
  a database's claim) before committing it - see `README.md` and any
  `test_roms/*/README.md` for the standard this project already holds
  itself to.
- Pair every bug fix with a regression test - a real ROM's observed
  behavior where one exists, a direct unit test where it doesn't (see
  `tests/test_apu.c`/`test_cpu.c`/`test_savestate.c` for recent
  examples of the latter).
- Document honest findings even when a fix doesn't achieve the hoped-for
  result (see `docs/GAMEBOY_ROADMAP.md`'s Phase 8 entry for a concrete
  example: a real, additive timing fix that turned out *not* to close
  `dmg-acid2`'s remaining visual gap, and says so directly rather than
  overstating the result).
