---
title: "8. Testing like you mean it"
nav_order: 9
---

# Testing like you mean it

At this point you have a working emulator. This chapter is about the
long climb from *working* to *correct* — and it's the part most
tutorials skip entirely. It's told through the reference project's
own arc from "98% of dmg-acid2, 24/44 Mooneye" to "100% and 80/83,"
because the individual fixes matter less than the method.

## The test pyramid that emerged

- **Direct unit tests** (`make test`-fast, no ROMs) for behavior a
  whole-ROM run can't isolate: a timer edge case, an MBC register, a
  save-state field. Written *alongside* the fix they guard, every time.
- **Hardware test ROMs** — [Mooneye](https://github.com/Gekkio/mooneye-test-suite)
  above all: MIT-licensed (committable, unlike Blargg),
  real-hardware-verified, with a simple pass/fail serial protocol, and
  **prebuilt binaries downloadable** from the author's site — no
  assembler toolchain needed. Adopt a curated subset near what you
  claim works; record a per-ROM PASS/FAIL baseline file, *including
  the FAILs*.
- **Real-software baselines** ([chapter 7](07-real-games.md)) —
  byte-exact frames and audio guarding everything at once.

Baselines are **floors, not targets**: the suite fails on any
regression below the recorded state, and an honest `FAIL` entry is a
recorded known gap, not an embarrassment. The reference project's
Mooneye baseline carried 20 expected failures on day one — clustered,
by reading each failing test's source, into five *named* causes, four
small and one architectural.

## Fix the cheap ones first

The four small clusters each fell to the standard loop — read the
test's actual source, ground the behavior in Pan Docs, fix, re-run
everything. One yielded a genuinely surprising find: real hardware
re-reads `IE & IF` *between the two bytes of the interrupt-dispatch
PC push*, so a push that overwrites `IE` can cancel the dispatch —
which also exposed that the emulator pushed the two bytes in the
wrong order (observably identical everywhere except exactly this
case).

## The architectural one: a story in three acts

Fourteen ROMs failed for one reason: OAM DMA was an instant copy, not
a timed 160-M-cycle transfer, and these tests deliberately race
instructions against the transfer window.

**Act one — targeted precision.** Rebuild DMA as a real M-cycle
pipeline, but have only the dozen opcodes the tests probe tick it at
true M-cycle granularity; everything else ticks lump-sum. First run:
0/14, unchanged. Root cause, traced rather than guessed: the
instruction that *starts* DMA (`ldh (FF46),a` — the very macro the
tests use) wasn't in the precise set, so the entire measured timeline
was offset two M-cycles. One addition: **13/14**. The 14th turned
out, on reading its source, to be a *timer*-precision test
misfiled by its name — never a DMA test at all. Know why each test
passes or fails; mislabeling a failure poisons your roadmap.

**Act two — the same trick fails on the timer.** Apply the identical
curated-precision approach to the timer for that 14th ROM: **ten
previously-passing ROMs regress.** The post-mortem is the deepest
design lesson in this guide: DMA's state doesn't depend on *when* you
observe it between memory accesses — the timer's edge-detection **is**
its counter value at an exact instant. Partial precision made that
value wrong at every register write a "precise" opcode performed.
Some subsystems tolerate selective accuracy; some are all-or-nothing.
The attempt was **reverted cleanly and the finding written down** —
a documented failed fix is real engineering output.

**Act three — pay full price.** Later, the full rewrite: every opcode
handler ticks every subsystem once per real M-cycle, mid-instruction,
with each handler's cycle placement derived from the official opcode
table. Result: the stubborn ROMs pass; one *new* regression appears —
a rapid-TAC-toggle edge case so delicate that real hardware revisions
disagree on it (the test's own header says DMG-0 units fail it).
After instrumented investigation and an independent reimplementation
of the reference algorithm converged on the same answer, it was
**accepted and recorded as a known FAIL** rather than reverting two
real fixes to dodge it. Perfection isn't the standard; *accounted-for*
is.

## The last 2% of dmg-acid2

The remaining PPU gaps fell one Mooneye ROM at a time, each a real
hardware mechanism the "close enough" model lacked: STAT interrupts
fire on the **rising edge of one shared, OR-ed line** (so one source
already high *blocks* another — "STAT blocking"); mode changes become
visible to register reads **one M-cycle late**; OAM is CPU-inaccessible
during modes 2–3, and VRAM during mode 3 — with a genuinely
*asymmetric* one-M-cycle bus handoff at the mode 2→3 boundary
(OAM writes unblock early, VRAM reads block early, their counterparts
don't — pinned down by testing all four combinations against the
ROMs' own data after two simpler hypotheses failed).

The VRAM-blocking fix — landed for a Mooneye ROM, not for acid2 —
took **dmg-acid2 from 99.71% to 100.00%** as a side effect. That's
the quiet thesis of this chapter: chase *mechanisms*, not pixels, and
the pixels fix themselves.

## Save states as test infrastructure

Add save states ([chapter 9](09-frontend.md)) and test them the
paranoid way: a unit test that sets every field distinctive, saves,
stomps, loads, and checks *every field individually* — plus a
three-process real-ROM test proving save/load lands byte-identical to
an uninterrupted run. Two bonuses: the field-by-field test catches
any new hardware state you forget to serialize (the reference
project's format reached version 14 as timing state grew), and a bit-exact
`--save-state`/`--load-state` pair becomes a *test tool* — snapshot
mid-scenario, continue in a fresh process, bisect long-running
behavior without replaying from boot.

Next: [a real front end](09-frontend.md).
