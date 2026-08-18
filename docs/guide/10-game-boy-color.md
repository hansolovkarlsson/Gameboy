---
title: "10. Game Boy Color"
nav_order: 11
---

# Game Boy Color

With a mature DMG emulator, CGB support is a well-bounded extension —
the CPU is the same SM83, and every subsystem you built carries over.
The reference project added the full rendering pipeline in one scoped
pass, then double-speed mode, HDMA, and the IR register as follow-ups.

## Mode detection

Header byte `0x0143` says whether a cartridge is CGB-enhanced (`0x80`),
CGB-only (`0xC0`), or DMG-only. Resolve **one explicit runtime flag**
at load ("is this run in CGB mode?") and branch on it — don't infer
mode implicitly around the codebase. CGB mode changes the post-boot
CPU registers too (`A = 0x11` is how games detect a CGB — Pan Docs,
["Power-Up Sequence"](https://gbdev.io/pandocs/Power_Up_Sequence.html)).

## The rendering slice

Four pieces make color rendering (Pan Docs,
["CGB Registers"](https://gbdev.io/pandocs/CGB_Registers.html),
["Palettes"](https://gbdev.io/pandocs/Palettes.html),
["Tile Maps"](https://gbdev.io/pandocs/Tile_Maps.html)):

- **WRAM banking** — `SVBK` (`FF70`) switches `D000–DFFF` among banks
  1–7 (writing 0 selects 1). Keep banks as separate arrays indexed at
  access time; don't copy bytes into a flat window.
- **VRAM bank 1** — `VBK` (`FF4F`). Bank 0 stays tile data/maps; bank
  1 holds more tile data *and the attribute map*.
- **Per-tile attributes**: each BG map byte gains a shadow byte in
  VRAM bank 1 — palette 0–7, tile-data bank, X/Y flip, BG-to-OBJ
  priority. Sprites gain the same fields in OAM attributes.
- **Color palette RAM**: 8 BG + 8 OBJ palettes × 4 colors × RGB555,
  accessed through index/data register pairs (`BCPS/BCPD`,
  `OCPS/OCPD`) with auto-increment, and blocked during mode 3.

Priority also changes meaning: LCDC bit 0 becomes a master
BG-priority toggle rather than a BG-enable, and sprite-to-sprite
priority is OAM order, not X coordinate.

Two implementation notes that saved real pain:

- **Don't retrofit your DMG framebuffer into RGB.** 255/170/85/0
  gray levels don't survive an RGB555 round-trip exactly — unifying
  the pipelines would have drifted every committed DMG baseline a few
  gray levels for zero correctness gain. The reference emulator keeps
  a separate RGB555 framebuffer for CGB mode; every DMG reference
  stayed byte-exact.
- **RGB555 bit order**: red is bits 0–4, blue bits 10–14. The
  reference implementation had them swapped, and the acid test's face
  rendered cyan. Which is the point of the gate—

## The gate

[**cgb-acid2**](https://github.com/mattcurrie/cgb-acid2), the color
sibling of dmg-acid2: same face, every feature exercising a CGB
attribute/palette/priority behavior, byte-exact reference image. The
reference emulator passes **100%**; the red/blue swap above is
exactly the class of bug it exists to make unmissable.

## The follow-up features

**Double-speed mode** (`KEY1`, `FF4D`): games arm bit 0 then execute
`STOP`; the CPU doubles while PPU/APU stay at real-time rate. If your
architecture funnels everything through one per-M-cycle tick
([chapter 8](08-testing.md)'s rewrite), this lands in one place: DMA
and timer tick 1:1 with CPU M-cycles (they genuinely speed up —
matching hardware), while PPU/APU get half T-states per call. What
looked like the scariest CGB feature became a small, centralized
change — the payoff of that earlier architectural investment.

**HDMA/GDMA** (`FF51–FF55`): VRAM DMA. General-purpose mode copies a
block at once (CPU blocked); HBlank mode copies 16 bytes per HBlank,
riding the mode 3→0 transition your PPU already exposes. Watch the
read-back encoding: registers `HDMA1–4` are write-only; `HDMA5`
distinguishes "cancelled mid-transfer" (bit 7 set, remaining count in
bits 0–6) from "completed" (`0xFF`) — the reference implementation
initially collapsed those two states.

**Verification without ROMs**: no good license-clean test ROMs exist
for KEY1/HDMA. The answer was direct unit tests against Pan Docs'
literal numbers (2050 M-cycle speed-switch pause; 8 vs 16 M-cycles
per HDMA block) — plus a small self-written RGBDS assembly ROM
driving the real registers end-to-end, proving HBlank DMA completes
only if real HBlanks fire. When the ecosystem has no test, write the
missing one; it's your own code, so there's no license question.

Next: [going further](11-going-further.md).
