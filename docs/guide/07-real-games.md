---
title: "7. Real games"
nav_order: 8
---

# Real games

Test ROMs verify behaviors one at a time. Real games exercise
*combinations* no test author thought to combine — and every emulator
project learns things from real software that no suite taught. This
chapter is where yours becomes an emulator of games rather than of
test ROMs.

## Finding games you can legally test with

The homebrew scene is your supply. [Homebrew Hub](https://hh.gbdev.io/)
(backed by the [gbdev database](https://github.com/gbdev/database))
catalogs hundreds of games with license metadata — but **verify the
license at the actual linked repository, not the database's claim**,
before committing anything. The reference project's picks, each with a
committed license and a README documenting what it tests:

- [**2048-gb**](https://github.com/Sanqui/2048-gb) (zlib) — a complete
  puzzle game; small, deterministic, ideal first target.
- [**Tobu Tobu Girl**](https://github.com/SimonLarsen/tobutobugirl)
  (MIT) — a real action platformer, 8× larger ROM.
- **Droneboy** (MIT) — a drone synthesizer: sustained multi-channel
  audio from boot, the perfect `--wav` regression source.

The same search also produced a counter-example worth internalizing: a
music ROM whose database entry claimed zlib but whose creator's own
page stated no license at all. It stayed out. One standard,
consistently applied — including when the answer is inconvenient.

## Scripted input

You still have no keyboard. Add a `--input <script>` flag replaying
timed presses from a text file — `<frame> <BUTTON> <down|up>`, applied
at VBlank frame granularity (where a human's presses land anyway),
through the joypad API you designed in
[chapter 5](05-interrupts-timer-joypad.md). Twenty lines of parsing,
and suddenly *gameplay* is scriptable: the reference project's 2048
regression boots the game, presses Start, makes three moves, and
byte-compares the resulting frame — real game logic verified, sliding
tiles, merge, score and all.

This works because of a property worth guarding jealously:
**determinism**. No wall-clock time, no host randomness anywhere in
the core — the same ROM and script produce the same bytes every run,
so `cmp` is a complete test harness. (Games seed RNG from `DIV`; a
deterministic boot makes even that reproducible.)

## What real games actually found

Each of these produced a fix *plus a regression test*, and none came
from a test suite:

**A crash twelve seconds in** (Tobu Tobu Girl). Left idling, the game
jumped into WRAM and died on an illegal opcode. Root cause: the
`EI; HALT` idiom — flagged in [chapter 5](05-interrupts-timer-joypad.md)
— was being handled as the generic HALT bug, pushing a wrong interrupt
return address and double-executing one instruction, silently
corrupting the stack by two bytes. The crash surfaced thousands of
instructions later, nowhere near the cause. The fix came with direct
unit tests asserting the exact return address and stack balance — not
just "the game stops crashing."

**Faders that didn't fade** (Droneboy). Two volume sliders worked,
two didn't — leading straight to the APU's zombie mode
([chapter 6](06-apu.md)), with the game's own source citing the
technique by name. Interactive use finds what frame captures can't.

**The header that wouldn't load** (2048-gb) — the RAM-size-code
`0x01` story from [chapter 3](03-memory-and-cartridges.md). The first
real game tried wouldn't even boot on a header technicality.

**Flicker only a human could see.** Watching dmg-acid2 run *live*
(once a front end existed) revealed the image cycling between four
distinct states — every still-frame capture had sampled one point of
the cycle and looked stable. Single-frame regression baselines have a
blind spot; occasionally watch your emulator actually run.

## Turn every discovery into a baseline

Each finding above ended as a committed, `make`-runnable regression:
a locked title-screen frame, a locked post-merge gameplay frame, a
locked 2-second WAV, unit tests for the CPU fix. That's the
compounding loop that carries the whole project:

> real software → bug → root cause against a primary source → fix →
> baseline locked forever.

One honest caveat about byte-exact baselines: they're *brittle by
design*. A later, genuinely-correct timing improvement will shift a
DIV-seeded RNG draw or an audio trace, and baselines will "fail."
The rule the reference project holds: **re-verify by hand, then
re-lock — never blindly recapture.** For its 2048 baseline that meant
re-confirming the merge and score were still correct and only tile
positions moved, before committing the new reference. A baseline you
recapture without looking is worse than no baseline.

Next: [testing like you mean it](08-testing.md) — the chapter where
the remaining 2% of dmg-acid2 finally falls.
