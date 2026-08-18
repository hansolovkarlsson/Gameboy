---
title: "9. A real front end"
nav_order: 10
---

# A real front end

Everything so far runs headless, on purpose. Now make it playable by
a human — and notice how little that takes when the core was built
clean: the front end is one file that links the unchanged core
sources and provides video, keys, audio, and a main loop.

## Choose the toolkit for your output shape

The Game Boy's output is a raw pixel framebuffer. **SDL2** is built
around exactly that — `SDL_Texture` blits pixel buffers,
`SDL_QueueAudio()` accepts PCM samples, keyboard events are trivial,
and it's portable. The reference project actually built its first
front end on GTK4 + Cairo + CoreAudio — a toolkit inherited from a
sibling project rather than chosen for this one — and later replaced
it wholesale with SDL2 at a fraction of the code, losing nothing it
used. Match the toolkit to the output shape from the start.

The whole shape of
[`sdl/src/main.c`](https://github.com/hansolovkarlsson/gameboy/blob/main/sdl/src/main.c):

- **Video**: step the core one video frame (70224 T-states), upload
  the 160×144 framebuffer to a texture, draw it
  **nearest-neighbor-scaled 4×** — integer-scaled crisp pixels, never
  bilinear smear.
- **Loop timing**: emulating a frame takes microseconds; a ~16 ms
  delay per iteration paces wall-clock speed to ≈59.7 Hz.
- **Keys**: arrows = D-pad, Z/X = B/A, Enter = Start, Right Shift =
  Select — the layout BGB and SameBoy popularized. Feed the joypad
  API from [chapter 5](05-interrupts-timer-joypad.md).
- **Audio**: your APU already produces samples ([chapter 6](06-apu.md));
  each frame, hand the ~738 stereo pairs it accumulated to
  `SDL_QueueAudio()`. Producer and consumer pace off the same tick —
  no ring buffer needed.

Keep it out of your default build (it's your only external
dependency); the headless driver remains what CI runs.

## Save states

The feature that makes an emulator *nice*, and — as
[chapter 8](08-testing.md) showed — a test tool. Design notes that
proved right:

- **Serialize field by field, explicitly little-endian.** Never
  `memcpy` structs: they contain pointers, padding, and
  host-byte-order, none of which belong on disk.
- **Serialize everything the step functions consume** — including
  internal state like DMA pipeline progress and the PPU's mode
  counters, not just the architectural registers. (Skip driver-owned
  things like the audio output buffer — that's not hardware state.)
- **Refuse mismatched loads.** Store the ROM's size and a content
  hash; check before restoring, or a state from one game silently
  corrupts another.
- **Version the format** and bump on every new field.

Bind F5/F9 to save/load `<rom>.state`, and mirror the same paths as
`--save-state`/`--load-state` CLI flags on the headless driver so
tests exercise the identical code.

One last front-end payoff already banked in
[chapter 7](07-real-games.md): watching games run live is itself a
test method — the four-state flicker and the dead volume faders were
both found by a human at the window, not by a script.

Next: [Game Boy Color](10-game-boy-color.md).
