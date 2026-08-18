---
title: "11. Going further"
nav_order: 12
---

# Going further

You have a correct, playable, color-capable Game Boy emulator. Some
directions the reference project took from here, and honest notes on
what it never closed.

## Write games for your own emulator

The most satisfying validation there is: your emulator becomes a
*platform*. Two real toolchains, both worth having:

- [**RGBDS**](https://rgbds.gbdev.io/) — the homebrew scene's standard
  assembler. Ideal for small, surgical test ROMs: the reference
  project used it to close validation gaps no licensed ROM covered
  (an MBC3 RTC exerciser, an HDMA exerciser), each a hundred lines of
  assembly driving real memory-mapped registers.
- [**GBDK-2020**](https://github.com/gbdk-2020/gbdk-2020) — C for the
  Game Boy. Dramatically faster for real game logic. Its GPL-with-
  linking-exception license explicitly imposes nothing on your
  compiled ROM.

The reference repo contains three complete original CGB games built
this way — a match-3 puzzle, a Zelda-like adventure, and a
Donkey Kong-style platformer
([`prism/`](https://github.com/hansolovkarlsson/gameboy/tree/main/prism),
[`wayfarer/`](https://github.com/hansolovkarlsson/gameboy/tree/main/wayfarer),
[`ascent/`](https://github.com/hansolovkarlsson/gameboy/tree/main/ascent)) —
each developed milestone by milestone against the emulator itself,
with every milestone locked in by the same scripted-input/byte-exact
regression machinery from [chapter 7](07-real-games.md).

Writing for the platform also finds emulator truths from the other
side. The best example: a "PPU bug" where frames captured right after
display-enable showed stale data. Long suspected as an emulator
timing flaw, it turned out — once properly investigated — to be the
*game's* bug: real CGB hardware hands control to the cartridge with
the LCD already on, so writing VRAM without disabling the display
first tears on real hardware too. The emulator had been right all
along. Suspicion must flow both directions.

## The honest remaining gaps

Every emulator has them; the difference is whether they're written
down. The reference project's, as recorded in its own baselines:

- **Three Mooneye timer tests** — the rapid-TAC-toggle edge case
  (where real hardware revisions themselves disagree) and two
  single-assertion reload-window precision cases. Investigated to the
  limit of what's resolvable without real-hardware cycle traces.
- **Four Blargg `dmg_sound` sub-tests** — wave-RAM mid-playback
  corruption behavior, deliberately unmodeled, flagged in the code
  since the APU was written.
- **No pixel-FIFO renderer** — scanline rendering plus accurate mode
  *durations* proved sufficient for 100% on both acid tests, but
  per-pixel mid-scanline effects some commercial games use would need
  the full FIFO.
- **Link cable and IR as communication** — register-level fidelity
  only; real peer-to-peer needs networking infrastructure that was
  never in scope.

## If you take one thing

The emulator itself was never the hard part — thousands of people
have written one. What makes the project worth doing carefully:

1. **Ground every behavior in a primary source**, and cite it where
   the code lives.
2. **Predict before you fix** — a diagnosis you can falsify teaches
   more than a patch that happens to work.
3. **Lock every discovery into a regression** the moment you make it.
4. **Write down what you didn't do** — documented simplifications and
   honest FAIL baselines are what let you keep moving fast years in.

The [full development history](../GAMEBOY_ROADMAP.md) — every phase,
bug, dead end, and fix that didn't achieve what it hoped — is the
long-form version of this guide, and the proof the method holds up
over a real project's lifetime.

Good luck. Go emulate.
