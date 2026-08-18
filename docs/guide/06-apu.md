---
title: "6. The APU"
nav_order: 7
---

# The APU

Sound is deliberately late in the order: a silent screen is a smaller
loss than a black one, and audio is the hardest subsystem to verify by
eye — because you can't. The answer, as ever, is to make it a file:
add a `--wav` flag that renders N seconds of audio to a standard PCM
WAV. Now audio output can be listened to, diffed byte-for-byte, and
locked into regression baselines like any frame.

## The hardware

Four channels, mixed into stereo (Pan Docs,
["Audio"](https://gbdev.io/pandocs/Audio.html),
["Audio Registers"](https://gbdev.io/pandocs/Audio_Registers.html)):

- **CH1, CH2 — pulse waves**, four duty cycles; CH1 adds a frequency
  sweep unit (with its own shadow register — the visible frequency
  and the sweep's working copy are not the same thing).
- **CH3 — wave**, playing 32 4-bit samples from wave RAM
  (`FF30–FF3F`).
- **CH4 — noise**, a 15- or 7-bit LFSR.

Cross-cutting machinery, shared by all channels: **length timers**
(auto-silence after N ticks), **volume envelopes**, **DACs** that can
be off independently of channels, and `NR50/51/52` master
volume/panning/power. The heartbeat is the **frame sequencer**: a
512 Hz clock stepping an 8-state cycle that ticks length (256 Hz),
envelopes (64 Hz), and sweep (128 Hz).

Two mechanism-level choices that echo the timer chapter's lesson:

- The frame sequencer isn't an independent clock — it's driven by a
  **falling edge of DIV bit 4**. Tie it to your real system counter
  and the documented `DIV`-write interactions with audio timing come
  along for free.
- The DAC maps digital 0–15 to an *inverted* analog slope, and the
  output has a documented high-pass filter. Implement the cited
  formulas rather than inventing a mixer.

## Real bugs from this stage, all instructive

**A routing bug, not an APU bug.** Blargg's `01-registers` test
failed — and the cause was in the MMU: the APU's register span had
been routed as two ranges split around `NR52`/wave RAM, silently
dropping the nine *unused* registers at `FF27–FF2F` through to flat
memory, where they read back writable instead of as `0xFF`. Route the
full contiguous `FF10–FF3F` span. When a subsystem test fails,
suspect the plumbing before the subsystem — and when you can, confirm
against the test's own source (Blargg's assembly is published) rather
than reverse-engineering from a failure code.

**A footnote read too literally, twice.** Pan Docs says length timers
are unaffected by APU power-off on DMG. First implementation: exclude
the length *registers* from the power-off clear. Wrong — the
registers' readable bits (like `NR11`'s duty field) do clear; it's
the *internal countdown* that survives. Second pass overcorrected:
"ignore all writes while off" — also wrong, because on DMG a length
write's *reload* reaches the internal counter even while powered off.
The register byte and the internal state it feeds are separate
circuits with separate power-off rules. Blargg's tests probe exactly
this distinction.

**Quirks as measurement instruments.** Several "obscure behavior"
notes in Pan Docs — an extra length clock when enabling length on the
wrong sequencer phase, "zombie mode" volume nudges via `NRx2`
writes — sound skippable. They aren't: Blargg's tests use them *as
measurement techniques* for otherwise-unreadable internal state, and
real software uses them too — the reference project found zombie mode
because a homebrew synthesizer's volume faders didn't work, and that
ROM's own source cited the technique by name. (Implement the variant
Pan Docs confirms for DMG; the fuller algorithm is explicitly
inconsistent across hardware revisions — modeling unconfirmed behavior
is guessing with extra steps.)

## The gate, honestly

Blargg's `dmg_sound` suite. The reference emulator, after all of the
above plus two later fixes (the sweep's negate-mode-exit kill; the
frame sequencer resetting to step 0 on power-on), stands at **8 of
12** — the remaining four cover wave-RAM's mid-playback corruption
behavior (deliberately unmodeled, flagged as such in the code since
day one) and one length-counter checksum chased to a narrow
frame-sequencer phase detail and honestly left open.

That's a real shape for an APU: correct register file, correct
mechanisms, sounds right, byte-exact deterministic output — with the
unmodeled corners *written down* rather than silently absent. A
regression baseline from a real music ROM ([next chapter](07-real-games.md))
then guards all of it.

Next: [real games](07-real-games.md).
