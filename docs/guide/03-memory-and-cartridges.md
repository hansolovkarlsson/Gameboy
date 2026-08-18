---
title: "3. Memory & cartridges"
nav_order: 4
---

# Memory and cartridges

With the CPU validated, replace the flat-memory scaffold with the real
address map, and add cartridge bank switching so real games can load
at all.

## The memory map

The SM83's 16-bit address space (Pan Docs,
["Memory Map"](https://gbdev.io/pandocs/Memory_Map.html)):

| Range | What |
|---|---|
| `0000–3FFF` | Cartridge ROM bank 0 (fixed) |
| `4000–7FFF` | Cartridge ROM, switchable bank |
| `8000–9FFF` | VRAM |
| `A000–BFFF` | Cartridge (external) RAM, switchable |
| `C000–DFFF` | Work RAM |
| `E000–FDFF` | Echo RAM — a mirror of `C000–DDFF` |
| `FE00–FE9F` | OAM (sprite attribute table) |
| `FEA0–FEFF` | Not usable |
| `FF00–FF7F` | I/O registers |
| `FF80–FFFE` | High RAM (HRAM) |
| `FFFF` | Interrupt enable register |

Write your MMU as one pair of functions — `read_byte(addr)` /
`write_byte(addr)` — that routes each range to its owner. Cartridge
ranges route to the cartridge module; I/O registers route to whichever
subsystem owns them (PPU, timer, APU…) as those come to exist. This
routing function ends up being the spine of the whole emulator, and
several later bugs turn out to live *here* rather than in any
subsystem: a range routed nowhere falls through to some default
behavior that's silently wrong.

Two behaviors worth getting right early because test ROMs check them:

- **Echo RAM is real.** Reads and writes at `E000–FDFF` must alias
  work RAM.
- **Unused bits and unmapped registers read back as 1.** Not zero —
  `1`. Every I/O register's unused bits, and fully-unmapped addresses
  like `FF03` and `FF08–FF0E`, read as set. Mooneye's
  `bits/unused_hwio-GS` test checks precisely this, register by
  register, and in the reference project it flushed out a half-dozen
  registers that got it wrong (`SC`, `IF`, `STAT` bit 7…).

## The cartridge header

Every ROM starts with a header at `0x0100–0x014F` (Pan Docs,
["The Cartridge Header"](https://gbdev.io/pandocs/The_Cartridge_Header.html)):
title, cartridge-type byte (which mapper chip), ROM size code, RAM
size code, checksums. Parse it on load; the cartridge-type byte tells
you which MBC to emulate.

A real-world caution from the reference project: the very first
homebrew game it tried to run declared RAM size code `0x01` —
officially "unused" — and the loader rejected the ROM outright.
Pan Docs itself explains that various homebrew ROMs carry `0x01` by
tool mistake and typically use no cartridge RAM at all. Treating it as
"0 banks" instead of an error is the compatible choice. Header fields
are *claims*, and real software's claims are sometimes sloppy.

## MBCs: bank switching

A 16-bit bus can see 32 KB of ROM; most games are bigger. Cartridges
solve this with a **Memory Bank Controller** — a chip in the cartridge
that intercepts *writes to ROM addresses* (which would otherwise be
meaningless) as commands to switch which ROM/RAM bank appears in the
switchable windows. Three mappers cover the overwhelming majority of
software:

**MBC1** (Pan Docs, [MBC1](https://gbdev.io/pandocs/MBC1.html)) — a
5-bit ROM bank register plus a 2-bit secondary register that, per a
mode flag, either extends ROM addressing or selects the RAM bank. Its
famous quirk: **writing 0 to the bank register selects bank 1** — the
translation happens at *read time* on the 5-bit register value.

**MBC3** (Pan Docs, [MBC3](https://gbdev.io/pandocs/MBC3.html)) — adds
a battery-backed real-time clock. The RAM-bank register doubles as an
RTC register selector (values `0x08–0x0C`), and the RTC has a
latch: writing `0x00` then `0x01` to `6000–7FFF` freezes a snapshot of
the clock registers for tear-free reads.

**MBC5** (Pan Docs, [MBC5](https://gbdev.io/pandocs/MBC5.html)) — a
clean 9-bit ROM bank number, and — crucially — **no bank-0
translation quirk at all**. Pan Docs says it outright: "Writing 0 will
indeed give bank 0 on MBC5, unlike other MBCs."

That last sentence isn't trivia. The reference project initialized
every MBC's bank register to zero on load, which *looks* correct
everywhere — MBC1/MBC3's read-time quirk silently covers for it. On
MBC5 there's no quirk to cover, and all eight Mooneye MBC5 ROMs
crashed at boot: they call into bank-1 library code before ever
writing the bank register, relying on the real chip's **power-on value
of 1**. The fix was one line; *finding* it took tracing the program
counter into a field of `0xFF` padding. Verify power-on state against
a source (mooneye-gb's `Mbc5State::default()` — itself checked against
a genuine MBC5 chip), not against "zero seems fine."

## How to test this chapter

Banking has a testing problem: Blargg's CPU ROMs are all unbanked
32 KB — they exercise *none* of this. Two-part answer, and it's the
first real appearance of this guide's testing philosophy:

1. **Direct unit tests now.** Pan Docs' MBC pages include worked
   addressing examples. Turn them into unit tests that call your
   cartridge code directly — bank register writes in, expected
   ROM/RAM offsets out — covering MBC1's both modes, the bank-0
   quirks, RAM enable/disable, MBC3's latch sequence, MBC5's 9-bit
   number. The reference project's
   [`tests/test_cart.c`](https://github.com/hansolovkarlsson/gameboy/blob/main/tests/test_cart.c)
   is exactly this.
2. **Mooneye's `emulator-only/mbc1` and `mbc5` ROM suites later.**
   Real, MIT-licensed, committable test ROMs, verified against real
   chips. They're what caught the MBC5 power-on bug — independent
   test content probing assumptions your own unit tests share with
   your implementation.

Next: [the PPU](04-ppu.md) — pixels at last.
