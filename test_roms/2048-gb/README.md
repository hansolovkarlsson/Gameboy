# 2048-gb

Fetched from <https://sanqui.rustedlogic.net/etc/2048.gb> (linked from
<https://github.com/Sanqui/2048-gb>'s own README as the prebuilt
release), zlib/libpng-licensed (`LICENSE`, Copyright (c) 2014 "Sanqui")
- like `test_roms/dmg-acid2/`, this one has an explicit permissive
license, so it's safe to keep here. A real, unmodified, complete
homebrew game (a Game Boy port of the 2048 sliding-tile puzzle), used
for `docs/GAMEBOY_ROADMAP.md`'s Phase 6 real-game validation - the
Game Boy side's counterpart to the CP/M side's Tasty Basic/MBASIC/BDS C
real-software testing (`cpm/docs/ROADMAP.md`'s Phase 3).

**A real bug found and fixed getting this ROM to load at all**: its
cartridge header declares RAM size code `0x01`, which
`src/cart.c` rejected outright before this phase - pandocs'
`The_Cartridge_Header.md` documents `0x01` as officially "Unused" but
also that "Various 'PD' ROMs... are known to use the `$01` RAM Size
tag, but this is believed to have been a mistake with early homebrew
tools, and the PD ROMs often don't use cartridge RAM at all" - exactly
this ROM's situation (cart type `0x03`, MBC1+RAM+BATTERY, but no actual
save-game behavior was ever observed in testing). Fixed by treating
code `0x01` as 0 RAM banks rather than a load error - see `cart.c`'s
`ram_banks_for_code()`.

**Regression coverage** (`make gameboy-2048-test`): `input_script.txt`
is a scripted button sequence (see `main.c`'s `--input`, added this
phase specifically to make this kind of test possible - no real
interactive front end exists yet, that's Phase 7) that starts a new
game and plays it deep enough to trigger a real tile merge (two `2`
tiles combining into a `4`, with the score updating to match) -
verified by hand against the rendered frame before being locked in as
`reference_frame.ppm`, a known-good baseline. The run is fully
deterministic (confirmed byte-identical across repeated runs - this
emulator has no host-timing-derived randomness anywhere in its reset
path), so the test is a plain byte-for-byte `cmp` against that
baseline, not a fuzzy match - it exists to catch a future regression,
not to independently prove correctness the way dmg-acid2's external
reference image does.

**Reference recaptured once, deliberately** (the per-M-cycle CPU
rewrite - see `docs/GAMEBOY_ROADMAP.md`'s own entry): this game seeds
its tile-spawn RNG from a single `LDH A,(DIV)` read (confirmed by
scanning the ROM - exactly one `$F0 $04` occurrence), so the *exact*
T-state DIV is read at is part of this test's own determinism, not
incidental to it. The per-M-cycle rewrite made that read genuinely more
precise (every opcode now advances the timer per real M-cycle, not in
a lump sum after the whole instruction), which changed the RNG draw at
that point and, cascading through 180 frames of scripted play, the
tile-spawn positions in the final frame - reconfirmed by hand exactly
the same way as the original capture: still one merge (two `2`s into a
single `4`), still score `00004`, only the tiles' board positions
differ. Recaptured rather than treated as a regression, since the
*old* baseline was never a ground truth to begin with (unlike Mooneye's
real-hardware-verified assertions or dmg-acid2's reference image) -
just a snapshot of this emulator's own prior, less precise output.

**Recaptured a second time** (implementing VRAM access blocking during
Mode 3, previously missing entirely - see `docs/GAMEBOY_ROADMAP.md`'s
"LCD-enable line 0 quirk" entry): this game writes to VRAM during
Mode 3 in normal operation, previously always succeeding incorrectly;
now genuinely timing-sensitive, which shifted the same `LDH A,(DIV)`
RNG read's timing and, cascading through the same 180 scripted frames,
the tile-spawn positions again - reconfirmed the same way as both
times before: still one merge (two `2`s into a single `4`), still
score `00004`, only board positions differ. dmg-acid2's own pixel-match
rate going from 99.71% to a clean 100.00% in the same pass is
independent, external confirmation this was a real correctness fix,
not a regression being rationalized away.
