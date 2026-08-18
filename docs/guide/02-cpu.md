---
title: "2. The CPU (SM83)"
nav_order: 3
---

# The CPU: Sharp SM83

The Game Boy's CPU is commonly called the "LR35902" and often
described as "a Z80." It is Z80-*derived*, and treating it as an
actual Z80 will sink you. The real differences, all of which matter:

- **No `IX`/`IY` index registers** — none of the `DD`/`FD`-prefixed
  instructions exist.
- **No alternate register set** — no `EXX`, no `EX AF,AF'`.
- **No `ED`-prefixed block instructions** (`LDIR`, `CPIR`, …).
- **No `IN`/`OUT`** — all I/O is memory-mapped at `0xFF00`–`0xFF7F`.
- **New instructions the Z80 lacks**, most importantly the
  auto-increment/decrement loads `LD (HL+),A` / `LD (HL-),A` /
  `LD A,(HL+)` / `LD A,(HL-)`, which real Game Boy code uses
  constantly, plus `STOP`, `SWAP`, and a different `ADD SP,e8`.
- **`DAA` behaves differently** — verify it against a real reference
  rather than porting Z80 semantics.
- Only four flags (Z, N, H, C) in the F register's high nibble; the
  low nibble always reads zero.

So: write your own SM83 core from scratch. Don't try to parameterize
an existing Z80 core — the reference project considered exactly that
(it grew out of a Z80/CP-M emulator) and concluded the differences are
pervasive enough that a shared dispatch table means conditional logic
threaded through nearly every opcode handler. Full details in the
repo's [CPU reference](../CPU_REFERENCE.md).

## Structure

The classic shape works well:

- A `struct` holding registers (`A F B C D E H L`, `SP`, `PC`), the
  interrupt master enable flag, and cycle bookkeeping.
- **Table-driven dispatch**: a 256-entry table for unprefixed opcodes
  and a second 256-entry table for the `0xCB` prefix. The instruction
  set has large, fully regular blocks (`LD r,r'` is one formula across
  64 opcodes; the ALU groups and the entire CB table decode
  mechanically from bit fields), so you can write generic handlers for
  those and individually-named handlers only for the irregular rest.
  The reference [`cpu.c`](https://github.com/hansolovkarlsson/Gameboy/blob/main/src/cpu.c)
  covers all 512 opcodes with about 35 distinct handler functions.
- An ALU module for the flag-setting arithmetic — half-carry logic is
  fiddly enough to want in exactly one place.

Check **every opcode's byte length, cycle count, and flag effects**
against the [official table](https://gbdev.io/gb-opcodes/optables/) as
you implement it — not from memory, and not from a mirror. Doing
exactly that caught a real erratum during the reference
implementation: a widely-mirrored community JSON dataset lists
`BIT b,(HL)` at 16 cycles, but the correct value is 12 — `BIT` only
reads, so it skips the write-back cycle the other CB-prefixed
read-modify-write ops pay. That wrong value would have silently skewed
every timing test you'd ever run.

## The traps everyone hits

- **`DAA`.** Decimal-adjust after subtraction, the N flag, and the
  half-carry interactions are the classic source of "one instruction
  fails Blargg." Implement it from a careful reference description,
  then let the test ROM judge.
- **The rotate pairs.** `RLCA`/`RLA`/`RRCA`/`RRA` (one byte, Z flag
  always cleared) are *different instructions* from CB-prefixed
  `RLC A`/`RL A`/… (Z flag set from the result).
- **`ADD SP,e8` and `LD HL,SP+e8`** set H and C from the *unsigned
  low-byte* addition, regardless of the operand's sign. Z is always
  clear.
- **The HALT bug.** With interrupts globally disabled but one already
  pending, `HALT` doesn't halt — the CPU fails to increment PC and
  executes the following byte *twice* (Pan Docs,
  ["halt"](https://gbdev.io/pandocs/halt.html)). You can't implement
  it properly until interrupts exist ([chapter 5](05-interrupts-timer-joypad.md)),
  but reserve a flag for it now. And note for later: the
  `EI; HALT` sequence is a *different* sub-case again — a real game
  crashed the reference emulator over exactly that distinction
  (the full story is in [chapter 7](07-real-games.md)).

## The correctness gate

Your CPU gate is **Blargg's `cpu_instrs`** suite (widely available;
see the [licensing note](01-before-you-start.md#the-legal-line) about
committing it). The individual sub-tests report pass/fail over the
serial port registers (`0xFF01`/`0xFF02`) as plain ASCII — so before
you have any graphics at all, a two-line hook in your MMU that
captures serial writes to stdout gives you a working test harness.

You don't need the full machine to start testing: a flat 64 KB memory
array with echo-RAM mirroring and that serial hook is enough to run
unbanked 32 KB ROMs. That's deliberately throwaway scaffolding — the
real MMU is [the next chapter](03-memory-and-cartridges.md) — but it
lets the CPU be validated *now* instead of after three more subsystems
exist.

Expect this result, and don't chase it: **10 of 11 sub-tests pass;
`02-interrupts` fails, and so does the separate `instr_timing.gb`.**
Both need a working timer and interrupt controller, which you haven't
built yet. The reference project hit exactly this split, predicted the
cause, moved on — and both flipped to passing the moment chapter 5's
work landed. Knowing *why* a test fails is the standard to hold;
"all green" comes later.

Next: [memory and cartridges](03-memory-and-cartridges.md).
