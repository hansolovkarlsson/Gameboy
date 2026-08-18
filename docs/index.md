---
title: Home
nav_order: 1
permalink: /
---

# Build a Game Boy Emulator

A step-by-step guide to writing an emulator for the original 1989
Game Boy (DMG) in plain C, for any hobbyist who wants to attempt it
themselves.

This isn't a theoretical tutorial. It was written alongside a
[real, working emulator](https://github.com/hansolovkarlsson/Gameboy)
that passes [dmg-acid2](https://github.com/mattcurrie/dmg-acid2) and
[cgb-acid2](https://github.com/mattcurrie/cgb-acid2) at 100%, passes
80/83 of a committed [Mooneye](https://github.com/Gekkio/mooneye-test-suite)
hardware-test subset, runs real homebrew games byte-for-byte
deterministically, and has full Game Boy Color support. Every chapter
teaches from that project's actual development history — including the
real bugs, the wrong turns, and the fixes that didn't work — rather
than pretending everything compiled correctly the first time.

## Why the Game Boy?

It's arguably the best first emulation project there is:

- **The hardware is completely documented.** [Pan Docs](https://gbdev.io/pandocs/)
  is a community-maintained reference covering every register, every
  timing quirk, every cartridge mapper — you never have to guess.
- **The test ecosystem is extraordinary.** Freely available test ROMs
  (Blargg's suites, Mooneye, dmg-acid2) will tell you *exactly* which
  hardware behavior you got wrong, often down to the specific register
  bit. No other retro platform hands you a correctness oracle this good.
- **It's tractable but not trivial.** A single 4 MHz CPU, a 160×144
  screen, four sound channels. You can hold the whole machine in your
  head — and still spend as long as you like chasing single-T-state
  timing accuracy if that's your idea of fun.
- **There's a thriving homebrew scene** producing open-source,
  permissively-licensed games you can legally test against — and once
  your emulator works, you can write games *for* it.

## The guide

The chapters follow real dependency order — each stage is testable on
its own before the next begins:

1. [Before you start](guide/01-before-you-start.md) — sources, scope,
   architecture, and the two rules that make the whole thing work.
2. [The CPU](guide/02-cpu.md) — the Sharp SM83, which is *not* a Z80.
3. [Memory and cartridges](guide/03-memory-and-cartridges.md) — the
   memory map and MBC bank switching.
4. [The PPU](guide/04-ppu.md) — tiles, sprites, and the scanline
   state machine that makes pixels.
5. [Interrupts, timer, and joypad](guide/05-interrupts-timer-joypad.md)
   — the machinery that makes games *playable*.
6. [The APU](guide/06-apu.md) — four channels of real sound.
7. [Real games](guide/07-real-games.md) — validation against actual
   software, and the bugs only real games find.
8. [Testing like you mean it](guide/08-testing.md) — the discipline
   that separates "it boots" from "it's correct."
9. [A real front end](guide/09-frontend.md) — SDL2, live audio, and
   save states.
10. [Game Boy Color](guide/10-game-boy-color.md) — banking, palettes,
    double speed, and HDMA.
11. [Going further](guide/11-going-further.md) — writing homebrew for
    your own emulator, and the gaps that remain.

## Reference material

Three companion references, maintained in the same repository, go
deeper than the tutorial needs to:

- [The project roadmap](GAMEBOY_ROADMAP.md) — the complete, honest
  development history this guide is distilled from: every phase, every
  bug, every fix that didn't achieve what it hoped.
- [CPU reference](CPU_REFERENCE.md) — the SM83 instruction set and
  where it genuinely differs from a Z80.
- [Hardware reference](HARDWARE_REFERENCE.md) — memory map, MBCs,
  PPU, APU, timer, joypad, and CGB, each claim cited to a real
  Pan Docs page.
