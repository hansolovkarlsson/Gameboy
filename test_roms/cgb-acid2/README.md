# cgb-acid2

Fetched from <https://github.com/mattcurrie/cgb-acid2> (v1.1 release
ROM `cgb-acid2.gbc` + `img/reference.png`), MIT-licensed (`LICENSE`,
Copyright (c) 2020 Matt Currie) - the CGB counterpart to this project's
own `test_roms/dmg-acid2/`, same author, same permissive license,
confirmed via `gh api repos/mattcurrie/cgb-acid2` before committing.

The standard CGB PPU correctness test in the Game Boy dev community -
see `make gameboy-cgb-visual-test` (`tests/compare_frame_cgb.py`) to
run it and compare the result against `reference.png` pixel-for-pixel.
Unlike `reference-dmg.png` (grayscale, compared via
`tests/compare_frame.py`), `reference.png` is an indexed/PLTE color
PNG, needing its own small decoder (`compare_frame_cgb.py`) and a raw
RGB PPM (`--mode cgb`'s `P6` output, see `src/main.c`) rather than the
grayscale `P5` `--ppm` output DMG mode produces.

**Result: 23040/23040 pixels match (100.00%)** - a genuine full pass,
unlike `dmg-acid2`'s still-open small gap. Getting there took one real,
found-by-this-ROM bug: this project's first RGB555->RGB888 conversion
(`src/main.c`/`sdl/src/main.c`) had red and blue swapped (read as
`BBBBBGGGGGRRRRR` when pandocs' `Palettes.md` actually places red in
bits 0-4 and blue in bits 10-14) - invisible from code review alone,
since nothing about the bit-shift arithmetic looks wrong in isolation,
but glaringly obvious once rendered: the whole face came out cyan
instead of yellow. Fixed by swapping which shift feeds which channel;
see `docs/GAMEBOY_ROADMAP.md`'s CGB phase entry.

Per the ROM's own README: "Double speed mode and WRAM banking
emulation are not required" for this test - it specifically targets
palette/tile-attribute/priority/window rendering, not the pieces this
project's CGB work deliberately deferred (double-speed mode, HDMA/GDMA,
infrared).
