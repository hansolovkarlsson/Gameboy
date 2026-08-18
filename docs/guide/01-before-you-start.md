---
title: "1. Before you start"
nav_order: 2
---

# Before you start

Two rules did more for this project than any clever code, and they're
worth adopting before you write a line:

**Rule 1: Ground every hardware claim in a primary source. Never
guess.** The Game Boy is documented well enough that guessing is
strictly worse than looking it up — and the failure mode of guessing
isn't a crash, it's a subtly wrong emulator that works on 95% of
software and fails mysteriously on the rest. When you implement a
behavior, cite the source in a comment at the line that implements it.
Future-you, debugging at midnight, will want to know whether that
magic constant came from Pan Docs or from a hunch.

**Rule 2: Pair every bug fix with a regression test, written alongside
the fix — not after.** Where a real ROM exhibits the behavior, lock
its observed output in as a byte-exact baseline; where no ROM isolates
it cleanly, write a direct unit test against your own code. An
emulator accumulates dozens of delicate timing behaviors; without
tests, every improvement silently risks three regressions.

## Your primary sources

- [**Pan Docs**](https://gbdev.io/pandocs/) — *the* Game Boy hardware
  reference. Community-maintained, precise, honest about what's
  unconfirmed. Nearly every technical claim in this guide traces to a
  specific Pan Docs page.
- [**The official opcode table**](https://gbdev.io/gb-opcodes/optables/)
  — every instruction's encoding, byte length, cycle count, and flag
  effects, with the underlying data available as
  [JSON](https://gbdev.io/gb-opcodes/Opcodes.json). Beware mirrors:
  a commonly-copied community dataset lists `BIT b,(HL)` as 16 cycles
  when the correct value is 12. Check the official table.
- [**gbdev.io**](https://gbdev.io/) — the "Awesome Game Boy
  Development" list rounds up everything else: test ROMs, tools,
  tutorials, the homebrew database.
- [**mooneye-gb**](https://github.com/Gekkio/mooneye-gb) — Gekkio's
  reference emulator, whose behaviors are verified against real
  hardware. When Pan Docs doesn't pin something down precisely enough
  to implement, this project repeatedly resolved the ambiguity by
  reading mooneye-gb's source — a legitimate research technique, cited
  like any other source.

## What you're building

A Game Boy is a small set of cooperating subsystems, and your emulator
will mirror them one module per subsystem:

| Subsystem | What it does | Chapter |
|---|---|---|
| CPU (Sharp SM83) | Executes the game's code, ~4.19 MHz | [2](02-cpu.md) |
| MMU + cartridge | Routes the 16-bit address space; MBC bank switching | [3](03-memory-and-cartridges.md) |
| PPU | Draws 160×144 pixels, one scanline at a time | [4](04-ppu.md) |
| Interrupts, timer, joypad | VBlank/STAT/timer interrupts; input | [5](05-interrupts-timer-joypad.md) |
| APU | Four sound channels | [6](06-apu.md) |

The reference implementation keeps each of these in its own C file
([`src/`](https://github.com/hansolovkarlsson/Gameboy/tree/main/src):
`cpu.c`, `mmu.c`, `cart.c`, `ppu.c`, `timer.c`, `joypad.c`, `apu.c`),
with `main.c` as a thin headless driver. That last point matters more
than it looks:

**Build a headless core first, not a windowed app.** The reference
emulator's driver takes a ROM path plus flags — `--ppm` to dump a
rendered frame as an image, `--wav` to dump audio, `--input` to replay
scripted button presses from a text file, `--save-state`/`--load-state`
for snapshots. Every one of those exists for *testing*: a headless
core with file-based inputs and outputs can be regression-tested with
`cmp` in a Makefile. The real windowed front end ([chapter 9](09-frontend.md))
came *sixth*, after the core was already validated, and links the same
core sources unchanged.

## Scope decisions worth making up front

- **DMG first.** Target the original 1989 monochrome Game Boy. Game
  Boy Color is a clean extension later ([chapter 10](10-game-boy-color.md));
  starting with it doubles your surface area for no learning benefit.
- **Skip the boot ROM.** Real hardware runs a small internal boot ROM
  (the scrolling logo) before the cartridge. You can instead initialize
  CPU registers and hardware state to their documented post-boot values
  (Pan Docs, ["Power-Up Sequence"](https://gbdev.io/pandocs/Power_Up_Sequence.html))
  and jump straight to `0x0100`. One subtlety the reference project
  hit: the F register's initial carry/half-carry flags depend on
  whether the cartridge's *header checksum byte* is zero — not whether
  the checksum *validates*. Read the footnotes carefully.
- **Document simplifications where they live.** You will simplify (a
  fixed-length PPU mode, an instant DMA copy). Write the simplification
  down in a comment at the exact line, with what it affects. The
  reference project's whole history is those documented simplifications
  being replaced, one test ROM at a time — which only works if you can
  find them again.

## The legal line

Two categories of ROM, treated completely differently:

- **Open-source test ROMs and homebrew** — dmg-acid2, Mooneye, real
  homebrew games with real licenses. Safe to commit to your repo *after
  you verify the license against the actual linked repository*, not
  just a database's claim. Note what it is and where it came from in a
  README next to it.
- **Commercial ROM dumps** — Nintendo's copyrighted work, actively
  enforced. Never commit them, not even to a private repo. The
  reference project keeps a gitignored `roms/` directory for locally
  owned dumps; nothing in it ever leaves the machine.

There's also a middle category to respect: Blargg's test ROMs — the
classic CPU/sound suites — carry **no explicit license**. The
reference project used them locally for validation but never committed
them, and built its committable test gates from explicitly-licensed
alternatives (Mooneye is MIT) instead. Your repo, your call — but know
which category each file is in.

## Prerequisites

C (or any systems language — nothing here is C-specific), comfort with
bitwise operations and hexadecimal, and a way to view a PPM/PNG image.
No graphics or audio experience needed until the front-end chapter:
until then, "graphics output" means writing a pixel buffer to a file.

Next: [the CPU](02-cpu.md).
