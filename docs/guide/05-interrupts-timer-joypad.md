---
title: "5. Interrupts, timer, joypad"
nav_order: 6
---

# Interrupts, timer, and joypad

Everything so far makes a machine that *boots*. This chapter makes one
that *plays*: nearly every real game structures its main loop around
the VBlank interrupt, times gameplay with the timer, and — obviously —
reads buttons.

## Interrupt dispatch

Five interrupt sources, priority by bit order: VBlank, LCD STAT,
Timer, Serial, Joypad (Pan Docs,
["Interrupts"](https://gbdev.io/pandocs/Interrupts.html)). Three
pieces of state: `IE` (`FFFF`, which sources are enabled), `IF`
(`FF0F`, which are pending), and `IME` (the master switch, set by
`EI`/`RETI`, cleared by `DI`).

Your PPU and timer should already be *setting* `IF` bits. Dispatch is
the missing half: between instructions, if `IME` is set and
`IE & IF` is non-zero, take the lowest set bit — clear it, clear
`IME`, push `PC`, jump to that source's fixed vector
(`0x40/0x48/0x50/0x58/0x60`), 20 T-states.

The classic subtleties, every one of them probed by a test ROM:

- **`EI` is delayed one instruction** — `IME` turns on *after* the
  following instruction. And `DI` immediately after `EI` must win: the
  reference emulator applied the delayed enable unconditionally, so
  `EI; DI` briefly enabled interrupts — Mooneye's `rapid_di_ei` caught
  it.
- **The HALT bug**, now implementable for real: `IME` clear + an
  interrupt already pending at `HALT` means no halt, and the following
  byte executes twice (with real side effects — replay the
  instruction, don't just re-fetch).
- **`EI; HALT` is a different case again**: HALT is effectively
  canceled, the pending interrupt dispatches with HALT's own address as
  the return point, and `RETI` re-executes the HALT. Getting this
  wrong doesn't fail a test ROM first — it corrupts the stack by two
  bytes and crashes a real game *twelve seconds in*
  ([chapter 7](07-real-games.md) has that story).

## The timer: emulate the mechanism, not the description

`DIV`, `TIMA`, `TMA`, `TAC` (`FF04–FF07`). The naive reading —
"`TIMA` increments at one of four fixed rates" — will pass casual
inspection and fail hardware tests, because it's not how the circuit
works.

The real mechanism (Pan Docs,
["Timer Obscure Behaviour"](https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html)):
one free-running **16-bit system counter**, of which `DIV` is just the
visible upper byte. `TIMA` increments on the **falling edge of one
specific counter bit**, selected by `TAC`. Model it that way and two
genuinely obscure behaviors fall out *for free* instead of needing
special cases:

- **Writing `DIV`** (which resets the counter) can itself produce a
  spurious `TIMA` tick — if the monitored bit was high, resetting it
  is a falling edge. `STOP` resets the same counter; same effect.
- **`TIMA` overflow is delayed**: the reload from `TMA` and the
  interrupt request happen one M-cycle *after* the overflow, and
  `TIMA` reads `0x00` in between. Writes landing in that window have
  their own precise rules (a `TIMA` write in the overflow cycle
  cancels the reload; one during the reload cycle is ignored; a `TMA`
  write during the reload propagates immediately).

Choosing the falling-edge model up front is the single best design
decision available in this chapter. The reference project made it
before any test demanded it, then covered both quirks with direct unit
tests — and still spent later sessions on the *cycle-exact* corners
([chapter 8](08-testing.md)); the three hardest Mooneye timer tests
are the only ones it has never fully closed.

## The joypad

One register, `P1/JOYP` at `FF00`, multiplexing eight buttons in two
groups of four, selected by write bits — and everything is
**inverted: 0 means pressed** (Pan Docs,
["Joypad Input"](https://gbdev.io/pandocs/Joypad_Input.html)).
Implement the register plus a small API for a front end to feed real
key state into. No front end exists yet — that's fine. Design the API
now (`set_action_buttons()` / `set_direction_buttons()`), and
[chapter 7](07-real-games.md)'s scripted-input harness becomes its
first caller.

## The payoff

Re-run your gates from the previous chapters. The reference project
saw, in one step:

- Blargg's `02-interrupts` and `instr_timing`: **failing → passing** —
  exactly the two predicted in [chapter 2](02-cpu.md).
- dmg-acid2: **91.31% → 98.04%** — the raster effects predicted in
  [chapter 4](04-ppu.md) now actually fire.

That's the reward of the predict-then-verify habit: two chapters'
worth of "expected failures" flipping green simultaneously, confirming
the earlier diagnoses were knowledge rather than hope.

Next: [the APU](06-apu.md) — sound.
