# Game Boy emulator - directory layout

**Tutorial site**: [Build a Game Boy Emulator](https://hansolovkarlsson.github.io/Gameboy/)
- a step-by-step guide for hobbyists, distilled from this project's
real development history (published via GitHub Pages from `docs/`;
the guide's chapters live in `docs/guide/`).

## Playing

Build the SDL2 front end (`brew install sdl2`, then `make gameboy-sdl`)
and run a ROM in a real window:

```
bin/gameboy-sdl <rom.gb> [--mode dmg|cgb|auto]
```

Key bindings (see `sdl/src/main.c`'s `handle_key_down()` - the
BGB/SameBoy-style default layout, not invented here):

| Key | Game Boy |
|---|---|
| Arrow keys | D-pad |
| X | A |
| Z | B |
| Enter | Start |
| Right Shift | Select |
| F5 / F9 | Save state / load state (to/from `<rom>.state`) |

## Project

A standalone Game Boy emulator - both the original DMG (the 1989
monochrome hardware) and the Game Boy Color (CGB). Originally developed as a
subproject inside a Z80/CP-M emulator repo, then split out (via `git
subtree split`, preserving its real commit history) once it became
clear the two shared no code at all - see `docs/GAMEBOY_ROADMAP.md`'s
"Architecture decision" section for the reasoning behind keeping them
separate even before the split, and its Status section for exactly
when/how the split happened. Several comments throughout this codebase
still cite `cpm/...` paths from that sibling Z80/CP-M repo (now a
separate GitHub repository, not a directory here) as the real prior art
a given design decision was compared against or modeled on - those
citations remain accurate as "this is where the reasoning/precedent
came from", just no longer as "elsewhere in this same repo".

See `docs/GAMEBOY_ROADMAP.md` for project status and phases,
[`CPU_REFERENCE.md`](docs/CPU_REFERENCE.md) for the SM83 instruction
set, and [`HARDWARE_REFERENCE.md`](docs/HARDWARE_REFERENCE.md) for the
memory map, cartridge/MBC banking, PPU (graphics), APU (sound), timer,
and joypad. This file just covers the two ROM directories and why
they're treated differently.

- `src/` - the emulator source itself: own opcode table, own ALU code,
  no dependency on the sibling Z80/CP-M repo's emulator - see
  `docs/GAMEBOY_ROADMAP.md`'s "Architecture decision" section for why
  that repo's Z80 core and this one's SM83 core were kept as separate
  implementations even while they lived in the same repo.

- `test_roms/` - open-source correctness test suites (Blargg's
  `gb-test-roms`, the Mooneye GB test suite, etc.) - this project's own
  equivalent of the Z80/CP-M repo's ZEXALL/ZEXDOC exercisers. Safe to
  commit once fetched, same reasoning that sibling repo's
  `cpm/resources/bdsc/upstream/README.md` documents for BDS C: verify
  the actual license before adding anything here, and note where it
  came from.

- `roms/` - real cartridge dumps, gitignored (`roms/.gitignore`) and
  **never committed, not even to this private repo**. Unlike the
  sibling Z80/CP-M repo (where a judgment call was already made to keep
  a real dBASE II binary committed), commercial Game Boy ROMs are
  Nintendo's copyrighted work, actively and specifically enforced -
  meaningfully different risk than 1980s CP/M software whose publishers
  mostly no longer exist or have released it. Keep your own dumps here
  locally; they'll never leave your machine via this repo.

- `sdl/` - the real SDL2 front end (`make gameboy-sdl`, opt-in - the
  only build target with an external dependency beyond a bare C
  compiler), a live playable window (video, keyboard input, sound, and
  save states) rather than the `--ppm`/`--wav`/`--input` bring-up
  driver `src/main.c` still provides for testing. Replaced an earlier
  GTK4+Cairo+CoreAudio front end - see `sdl/src/main.c`'s own top
  comment and `docs/GAMEBOY_ROADMAP.md`'s Phase 7 status for the full
  reasoning, and the "Playing" section above for the key bindings.
