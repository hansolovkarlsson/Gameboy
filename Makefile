CC := gcc
CFLAGS := -Wall -Wextra -O2

BIN_DIR := bin

SRC_DIR := src
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:.c=.o)
TARGET := $(BIN_DIR)/gameboy

# cart.c's own unit tests (tests/test_cart.c) - unlike Blargg's
# cpu_instrs (fetched locally, never committed - see
# docs/GAMEBOY_ROADMAP.md's licensing note), this is this project's own
# code with no licensing question, so it's a real `make`-able regression
# gate for the MBC1/MBC3/MBC5 banking logic despite no real MBC test ROM
# being available to commit.
TEST_TARGET := $(BIN_DIR)/gameboy-test-cart

# timer.c's own unit tests (tests/test_timer.c), same reasoning
# as TEST_TARGET above but for the DIV/TAC-write "spurious
# tick" quirks and the TIMA overflow-reload delay - obscure enough to
# be worth a direct, ROM-independent check even though Blargg's
# instr_timing.gb (used locally, not committed) already exercises this
# timer broadly.
TEST_TIMER_TARGET := $(BIN_DIR)/gameboy-test-timer

# apu.c's own unit tests (tests/test_apu.c) - "zombie mode"
# volume-nudge writes, a real obscure DMG APU behavior a real ROM
# (test_roms/droneboy/) turned out to depend on. gb_apu_write()
# needs no GBCpu/mmu stub at all, same ROM-independent reasoning as
# TEST_TARGET/TEST_TIMER_TARGET above.
TEST_APU_TARGET := $(BIN_DIR)/gameboy-test-apu

# cpu.c's own unit tests (tests/test_cpu.c) - the "HALT
# immediately after EI" sub-case of the HALT bug, a real gap a real ROM
# (test_roms/tobutobugirl/) found. Needs mmu.c linked (for
# gb_read_byte/gb_write_byte) but no cart/ppu/timer/joypad/apu, same
# minimal-dependency reasoning as TEST_APU_TARGET above.
TEST_CPU_TARGET := $(BIN_DIR)/gameboy-test-cpu

# savestate.c's own unit test (tests/test_savestate.c) - a direct
# round-trip check (save a hand-built state, mutate every field, load,
# assert everything came back) - same minimal-dependency reasoning as
# TEST_CPU_TARGET above (needs cpu/mmu/cart/ppu/timer/joypad/apu linked
# since savestate.c reaches all of them through GBCpu, but no real ROM).
# SAVESTATE_TEST below is the complementary real-ROM/real-driver
# round-trip check (does a save+load actually resume a genuine run
# bit-identically), through the actual --load-state/--save-state CLI
# flags rather than the struct-level API directly.
TEST_SAVESTATE_TARGET := $(BIN_DIR)/gameboy-test-savestate

# dmg-acid2 (test_roms/dmg-acid2/ - MIT-licensed, committed
# unlike Blargg's ROMs) is the PPU's real correctness gate: render a
# frame, compare it pixel-for-pixel against the reference image.
# Informational against a regression floor rather than a hard 100%
# pass/fail - see docs/GAMEBOY_ROADMAP.md's Phase 4 status for the
# current match rate and its still-open remaining gap, and
# tests/compare_frame.py's own comment for the regression-baseline reasoning.
VISUAL_ROM := test_roms/dmg-acid2/dmg-acid2.gb
VISUAL_REF := test_roms/dmg-acid2/reference-dmg.png
VISUAL_OUT := $(BIN_DIR)/dmg-acid2-output.ppm

# cgb-acid2 (test_roms/cgb-acid2/ - MIT-licensed, same author as
# dmg-acid2) is CGB rendering's own correctness gate - see
# test_roms/cgb-acid2/README.md. Unlike dmg-acid2 (an open, still-
# documented gap), this one is a genuine 100% pixel-exact match.
CGB_VISUAL_ROM := test_roms/cgb-acid2/cgb-acid2.gbc
CGB_VISUAL_REF := test_roms/cgb-acid2/reference.png
CGB_VISUAL_OUT := $(BIN_DIR)/cgb-acid2-output.ppm

# Real-ROM save/load round-trip: run dmg-acid2 continuously to frame 2
# as the baseline, then separately run it to frame 1, save state, and
# in a *third*, fresh process load that state and run one more frame -
# if save/load fully captures everything gb_cpu_step()/gb_ppu_step()/
# etc. need to resume correctly, that third process's frame 1 output
# must be byte-identical to the continuous run's frame 2 (cmp, not
# compare_frame.py's percentage gate - a real round-trip has no excuse
# for even one differing byte). Reuses dmg-acid2 rather than a new ROM
# since it's already committed and deterministic with no scripted input
# needed.
SAVESTATE_CONTINUOUS := $(BIN_DIR)/savestate-continuous.ppm
SAVESTATE_MID_PPM := $(BIN_DIR)/savestate-mid.ppm
SAVESTATE_MID_STATE := $(BIN_DIR)/savestate-mid.state
SAVESTATE_RESUMED := $(BIN_DIR)/savestate-resumed.ppm

# 2048-gb (test_roms/2048-gb/ - zlib-licensed, committed same as
# dmg-acid2) is real-game validation: a genuine, unmodified third-party
# homebrew game, scripted (via main.c's --input) through starting a game
# and playing far enough to trigger a real tile merge, then diffed
# byte-for-byte against a known-good captured frame - see
# test_roms/2048-gb/README.md for the full story, including a real
# cartridge-loading bug this ROM found and fixed.
GB2048_ROM := test_roms/2048-gb/2048.gb
GB2048_SCRIPT := test_roms/2048-gb/input_script.txt
GB2048_REF := test_roms/2048-gb/reference_frame.ppm
GB2048_OUT := $(BIN_DIR)/2048-gb-output.ppm

# Droneboy (test_roms/droneboy/ - MIT-licensed, committed same as
# 2048-gb) is the live-audio counterpart to dmg-acid2's PPU test: real,
# sustained multi-channel sound from boot with no input needed, unlike
# 2048-gb's single startup blip - see test_roms/droneboy/README.md.
DRONEBOY_ROM := test_roms/droneboy/droneboy.gb
DRONEBOY_REF := test_roms/droneboy/reference_audio.wav
DRONEBOY_OUT := $(BIN_DIR)/droneboy-output.wav

# Tobu Tobu Girl (test_roms/tobutobugirl/ - MIT-licensed, committed
# same as 2048-gb) is a second real-game validation target: a well-known
# action/platformer rather than a puzzle game - see
# test_roms/tobutobugirl/README.md.
TOBU_ROM := test_roms/tobutobugirl/tobu.gb
TOBU_REF := test_roms/tobutobugirl/reference_frame.ppm
TOBU_OUT := $(BIN_DIR)/tobu-output.ppm

# RGBDS (rgbasm/rgblink/rgbfix, `brew install rgbds`) - opt-in, same
# external-dependency reasoning as GTK below. The chosen toolchain for
# any future custom Game Boy test content - see rgbds/README.md for the
# full reasoning.
RGBDS_HELLO_SRC := rgbds/examples/hello.asm
RGBDS_HELLO_OBJ := $(BIN_DIR)/rgbds-hello.o
RGBDS_HELLO_ROM := $(BIN_DIR)/rgbds-hello.gb

# MBC3's real-time clock, driven through the actual memory-mapped
# interface (bank-select, latch sequence, the shared $A000 window) a
# real MBC3+RTC game would use - see rgbds/examples/mbc3_rtc.asm's own
# top comment for why this is worth having alongside test_cart.c's
# synthetic-struct RTC checks.
RGBDS_MBC3_RTC_SRC := rgbds/examples/mbc3_rtc.asm
RGBDS_MBC3_RTC_OBJ := $(BIN_DIR)/rgbds-mbc3-rtc.o
RGBDS_MBC3_RTC_ROM := $(BIN_DIR)/rgbds-mbc3-rtc.gb

# CGB HDMA/GDMA (0xFF51-0xFF55), driven through the actual memory-mapped
# registers by genuinely CPU-executed code and, for the HBlank round,
# genuine PPU Mode 3->0 timing - see rgbds/examples/hdma.asm's own top
# comment for why this exists alongside tests/test_cpu.c's synthetic
# gb_hdma_hblank_trigger() calls (no real, permissively-licensed game or
# demo using HDMA was found).
RGBDS_HDMA_SRC := rgbds/examples/hdma.asm
RGBDS_HDMA_OBJ := $(BIN_DIR)/rgbds-hdma.o
RGBDS_HDMA_ROM := $(BIN_DIR)/rgbds-hdma.gb

# "Prism" (working title) - this project's first original homebrew
# game, written in GBDK-2020 (C) rather than RGBDS - see prism/README.md
# for the toolchain (not vendored here, same reasoning as RGBDS) and
# docs/GAMEBOY_ROADMAP.md for the milestone roadmap. Currently
# Milestone 8 (animated gem clear - a flash then a 2-stage shrink on
# the matched cells, prism/src/board.c's play_clear_animation(), on top
# of Milestone 7's animated gem swap, prism/src/swapanim.c) - the
# scripted --input sequence (prism/input_script_m7.txt, same mechanism
# test_roms/2048-gb's own regression test uses; unchanged since
# Milestone 7 - Milestone 8 adds no new input events, only more
# rendering after the last one) presses Start to dismiss the title
# screen (prism/src/title.c), attempts one deliberately non-matching
# swap first (exercising swapanim_play()'s revert/slide-back path)
# against the deterministic initial board (initrand() seeded from
# DIV_REG *before* title_screen()'s own player-paced wait, so the board
# is unaffected by it), then makes a real match-producing swap and
# confirms the HUD (prism/src/hud.c: score 0 -> 30, moves 20 -> 19), the
# select/revert/match sound effects (prism/src/sfx.c, via a second audio
# capture of the same run), and real cartridge-RAM persistence
# (prism/src/highscore.c, src/cart.c's gb_cart_load_ram_file()/
# gb_cart_save_ram_file()): that run's --sav output is cmp'd against a
# committed reference, then a *second*, separate invocation loads it
# fresh (no --input at all) and confirms the title screen now reads
# "HIGH 0030" instead of "HIGH 0000". The visual (.ppm) and .sav/title
# references are all unaffected by Milestone 8 - confirmed unchanged
# rather than assumed, since the resolved end state a captured frame
# far enough past the new clear animation shows is identical either
# way - only the audio reference needed re-locking in place (same
# by-now-familiar "added frames shift audio's exact sample position,
# not which frame anything lands on" category of thing).
PRISM_ROM := prism/bin/prism.gb
PRISM_SCRIPT := prism/input_script_m7.txt
PRISM_REF := prism/reference_m7.ppm
PRISM_OUT := $(BIN_DIR)/prism-output.ppm
PRISM_WAV_REF := prism/reference_m7_sfx.wav
PRISM_WAV_OUT := $(BIN_DIR)/prism-sfx-output.wav
PRISM_SAV_REF := prism/reference_m6c.sav
PRISM_SAV_OUT := $(BIN_DIR)/prism-output.sav
PRISM_TITLE_REF := prism/reference_m6c_title.ppm
PRISM_TITLE_OUT := $(BIN_DIR)/prism-title-output.ppm

# Wayfarer (wayfarer/ - a second, separate original homebrew game, a
# top-down action-adventure rather than prism/'s match-3 puzzle - see
# wayfarer/README.md and docs/GAMEBOY_ROADMAP.md's own entry). Milestone
# 11: a way out of a `won` save - Start on the win screen wipes it and
# restarts a fresh session (world.c's restart_game()), fixing a real
# gap Milestone 9 left open (a won save had no escape from the win
# screen). input_script_m8.txt/reference_m8.sav stay, unchanged, still
# feeding the separate won-reload check below (WAYFARER_WON_*) - that
# behavior isn't going anywhere, restart just adds an exit from it.
WAYFARER_ROM := wayfarer/bin/wayfarer.gb
WAYFARER_SCRIPT := wayfarer/input_script_m11.txt
WAYFARER_REF := wayfarer/reference_m11.ppm
WAYFARER_OUT := $(BIN_DIR)/wayfarer-output.ppm
WAYFARER_WAV_REF := wayfarer/reference_m11_sfx.wav
WAYFARER_WAV_OUT := $(BIN_DIR)/wayfarer-sfx-output.wav
WAYFARER_SAV_REF := wayfarer/reference_m11.sav
WAYFARER_SAV_OUT := $(BIN_DIR)/wayfarer-output.sav
WAYFARER_WON_SAV_SCRIPT := wayfarer/input_script_m8.txt
WAYFARER_WON_SAV_REF := wayfarer/reference_m8.sav
WAYFARER_WON_SAV_OUT := $(BIN_DIR)/wayfarer-won-input.sav
WAYFARER_WON_SCRIPT := wayfarer/input_script_m9_start.txt
WAYFARER_WON_REF := wayfarer/reference_m9_won.ppm
WAYFARER_WON_OUT := $(BIN_DIR)/wayfarer-won-output.ppm
# Milestone 12: a second, optional enemy - "the brute" (brute.c), a
# bigger (16x16 vs. the original 8x8) two-hit enemy in room (2,1), one
# of the two empty rooms Milestone 10 added. Not required to win - the
# win-condition check never consults it - so all the existing win-path
# references above stay completely untouched.
WAYFARER_BRUTE_SCRIPT := wayfarer/input_script_m12_brute.txt
WAYFARER_BRUTE_REF := wayfarer/reference_m12_brute.ppm
WAYFARER_BRUTE_OUT := $(BIN_DIR)/wayfarer-brute-output.ppm
WAYFARER_BRUTE_WAV_REF := wayfarer/reference_m12_brute_sfx.wav
WAYFARER_BRUTE_WAV_OUT := $(BIN_DIR)/wayfarer-brute-sfx-output.wav
WAYFARER_BRUTE_SAV_REF := wayfarer/reference_m12_brute.sav
WAYFARER_BRUTE_SAV_OUT := $(BIN_DIR)/wayfarer-brute-output.sav
# Captured mid-fight (frame 1000, brute still alive) rather than only
# post-defeat - a real gap the post-defeat-only frame above left open:
# a tile ID collision once corrupted 2 of the brute's 4 quadrant tiles
# (heart_hud.c's and key.c's own sprite tile IDs), invisible in the
# post-defeat frame (the brute is hidden by then) but plainly visible
# while alive. This check is what would have actually caught it.
WAYFARER_BRUTE_ALIVE_REF := wayfarer/reference_m12_brute_alive.ppm
WAYFARER_BRUTE_ALIVE_OUT := $(BIN_DIR)/wayfarer-brute-alive-output.ppm

# Milestone 14: a shield pickup (shield.c) in room (2,0), the last of
# the two rooms Milestone 10 left empty. Blocks contact damage, but
# only directionally (the player must be facing the threat's actual
# current position) - not required to win, purely a defensive upgrade.
WAYFARER_SHIELD_SCRIPT := wayfarer/input_script_m14_shield.txt
WAYFARER_SHIELD_REF := wayfarer/reference_m14_shield.ppm
WAYFARER_SHIELD_OUT := $(BIN_DIR)/wayfarer-shield-output.ppm
# Mid-approach, still genuinely blocked (2 hearts) - the counterpart to
# WAYFARER_SHIELD_REF's own post-transition state (1 heart), the same
# two-checkpoint shape WAYFARER_BRUTE_ALIVE_REF/WAYFARER_BRUTE_REF
# already establishes.
WAYFARER_SHIELD_BLOCKED_REF := wayfarer/reference_m14_shield_blocked.ppm
WAYFARER_SHIELD_BLOCKED_OUT := $(BIN_DIR)/wayfarer-shield-blocked-output.ppm
WAYFARER_SHIELD_WAV_REF := wayfarer/reference_m14_shield_sfx.wav
WAYFARER_SHIELD_WAV_OUT := $(BIN_DIR)/wayfarer-shield-sfx-output.wav
WAYFARER_SHIELD_SAV_REF := wayfarer/reference_m14_shield.sav
WAYFARER_SHIELD_SAV_OUT := $(BIN_DIR)/wayfarer-shield-output.sav

# Milestone 15: looping background music (music.c) on channel 3 (the
# wave channel, previously entirely unused) - a fixed melody that
# starts at world_init()/reset_world() and stops the moment the player
# wins. This script isolates the music alone (title dismiss, no other
# input) so its own reference doesn't depend on anything else changing.
WAYFARER_MUSIC_SCRIPT := wayfarer/input_script_m15_music.txt
WAYFARER_MUSIC_WAV_REF := wayfarer/reference_m15_music_sfx.wav
WAYFARER_MUSIC_WAV_OUT := $(BIN_DIR)/wayfarer-music-sfx-output.wav

# Milestone 16: an optional boss (boss.c) in a new dead-end room (2,2),
# grown south of the brute's own room (2,1) - the map is now 3x3, not
# 3x2. A 24x24 blob (bigger than the brute's own 16x16), bounces on
# both axes independently (a real first), takes three hits, and reuses
# brute.c's own OBJ palette (CGB only has 8 total, all already spoken
# for - see boss.c's own comment for the real bug this found and fixed
# before it shipped). Not required to win, same as the brute.
WAYFARER_BOSS_SCRIPT := wayfarer/input_script_m16_boss.txt
WAYFARER_BOSS_ALIVE_REF := wayfarer/reference_m16_boss_alive.ppm
WAYFARER_BOSS_ALIVE_OUT := $(BIN_DIR)/wayfarer-boss-alive-output.ppm
WAYFARER_BOSS_REF := wayfarer/reference_m16_boss.ppm
WAYFARER_BOSS_OUT := $(BIN_DIR)/wayfarer-boss-output.ppm
WAYFARER_BOSS_WAV_REF := wayfarer/reference_m16_boss_sfx.wav
WAYFARER_BOSS_WAV_OUT := $(BIN_DIR)/wayfarer-boss-sfx-output.wav
WAYFARER_BOSS_SAV_REF := wayfarer/reference_m16_boss.sav
WAYFARER_BOSS_SAV_OUT := $(BIN_DIR)/wayfarer-boss-output.sav

# Milestone 17: a treasure chest (chest.c) in room (2,0) - the
# shield's own room, the second precedent (after (0,0)'s enemy +
# sword_pickup) for two independent pickups sharing a room. Grants a
# permanent max-hearts increase (3 -> 4, player.c's own
# player_increase_max_hearts()) rather than just a full heal - real,
# lasting progression. Not required to win, same as the brute/boss.
WAYFARER_CHEST_SCRIPT := wayfarer/input_script_m17_chest.txt
WAYFARER_CHEST_COLLECTED_REF := wayfarer/reference_m17_chest_collected.ppm
WAYFARER_CHEST_COLLECTED_OUT := $(BIN_DIR)/wayfarer-chest-collected-output.ppm
# The counterpart checkpoint - one unarmed graze taken *after* growing
# to 4 max hearts, the real test that heart_hud.c's generalization
# renders partial damage correctly at the new width (3 full + 1 empty),
# not just "still shows 3 hearts total."
WAYFARER_CHEST_HIT_REF := wayfarer/reference_m17_chest_hit.ppm
WAYFARER_CHEST_HIT_OUT := $(BIN_DIR)/wayfarer-chest-hit-output.ppm
WAYFARER_CHEST_WAV_REF := wayfarer/reference_m17_chest_sfx.wav
WAYFARER_CHEST_WAV_OUT := $(BIN_DIR)/wayfarer-chest-sfx-output.wav
WAYFARER_CHEST_SAV_REF := wayfarer/reference_m17_chest.sav
WAYFARER_CHEST_SAV_OUT := $(BIN_DIR)/wayfarer-chest-output.sav

# Mooneye GB Test Suite (test_roms/mooneye/ - MIT-licensed, prebuilt
# ROMs committed same as dmg-acid2/2048-gb/droneboy/tobutobugirl, not
# built from source here - see test_roms/mooneye/README.md for the full
# story, including a correction to this doc's own Phase 1 note about
# what toolchain Mooneye actually needs). tests/run_mooneye.py runs
# every committed ROM and checks the real per-ROM baseline (24/44 pass
# - the other 20 trace to a handful of real, grounded, already-mostly-
# documented gaps, not committed as one-off fixes here) as a regression
# floor, the same reasoning tests/compare_frame.py already uses for
# dmg-acid2.
MOONEYE_DIR := test_roms/mooneye

# The real SDL2 front end (sdl/src/main.c) - opt-in, the only build
# target with an external dependency beyond a bare C compiler, and
# links the core directly instead of spawning a separate process (see
# sdl/src/main.c's own top comment for why - same reasoning the GTK4
# front end this replaced already established). Built from the core
# sources directly rather than $(OBJS), since that includes
# src/main.c's own competing main(). Only `sdl2` is needed.
CORE_SRCS := $(filter-out $(SRC_DIR)/main.c,$(SRCS))
CORE_OBJS := $(CORE_SRCS:.c=.o)
SDL_SRC_DIR := sdl/src
SDL_SRCS := $(wildcard $(SDL_SRC_DIR)/*.c)
SDL_OBJS := $(SDL_SRCS:.c=.o)
SDL_TARGET := $(BIN_DIR)/gameboy-sdl
SDL_PKGS := sdl2
SDL_CFLAGS := $(shell pkg-config --cflags $(SDL_PKGS) 2>/dev/null) -I$(SRC_DIR)
SDL_LIBS := $(shell pkg-config --libs $(SDL_PKGS) 2>/dev/null)

.PHONY: all gameboy gameboy-test gameboy-visual-test gameboy-cgb-visual-test gameboy-2048-test gameboy-droneboy-test gameboy-tobu-test gameboy-rgbds-test gameboy-rgbds-mbc3-test gameboy-rgbds-hdma-test gameboy-prism-build gameboy-wayfarer-build gameboy-savestate-test gameboy-mooneye-test gameboy-sdl clean

all: gameboy

gameboy: $(TARGET)

gameboy-test: $(TEST_TARGET) $(TEST_TIMER_TARGET) $(TEST_APU_TARGET) $(TEST_CPU_TARGET) $(TEST_SAVESTATE_TARGET)
	./$(TEST_TARGET)
	./$(TEST_TIMER_TARGET)
	./$(TEST_APU_TARGET)
	./$(TEST_CPU_TARGET)
	./$(TEST_SAVESTATE_TARGET)

gameboy-visual-test: $(TARGET)
	./$(TARGET) $(VISUAL_ROM) --ppm $(VISUAL_OUT) --frames 2
	python3 tests/compare_frame.py $(VISUAL_OUT) $(VISUAL_REF)

gameboy-cgb-visual-test: $(TARGET)
	./$(TARGET) $(CGB_VISUAL_ROM) --mode cgb --ppm $(CGB_VISUAL_OUT) --frames 2
	python3 tests/compare_frame_cgb.py $(CGB_VISUAL_OUT) $(CGB_VISUAL_REF)

gameboy-2048-test: $(TARGET)
	./$(TARGET) $(GB2048_ROM) --input $(GB2048_SCRIPT) --ppm $(GB2048_OUT) --frames 180
	cmp $(GB2048_OUT) $(GB2048_REF) && echo "gameboy-2048-test: OK (frame matches known-good reference)"

gameboy-droneboy-test: $(TARGET)
	./$(TARGET) $(DRONEBOY_ROM) --wav $(DRONEBOY_OUT) --seconds 2
	cmp $(DRONEBOY_OUT) $(DRONEBOY_REF) && echo "gameboy-droneboy-test: OK (audio matches known-good reference)"

gameboy-tobu-test: $(TARGET)
	./$(TARGET) $(TOBU_ROM) --ppm $(TOBU_OUT) --frames 60
	cmp $(TOBU_OUT) $(TOBU_REF) && echo "gameboy-tobu-test: OK (frame matches known-good reference)"

gameboy-savestate-test: $(TARGET)
	./$(TARGET) $(VISUAL_ROM) --ppm $(SAVESTATE_CONTINUOUS) --frames 2
	./$(TARGET) $(VISUAL_ROM) --ppm $(SAVESTATE_MID_PPM) --frames 1 --save-state $(SAVESTATE_MID_STATE)
	./$(TARGET) $(VISUAL_ROM) --load-state $(SAVESTATE_MID_STATE) --ppm $(SAVESTATE_RESUMED) --frames 1
	cmp $(SAVESTATE_CONTINUOUS) $(SAVESTATE_RESUMED) && echo "gameboy-savestate-test: OK (save/load round-trip is bit-exact against a continuous run)"

gameboy-rgbds-test: $(TARGET) | $(BIN_DIR)
	rgbasm -o $(RGBDS_HELLO_OBJ) $(RGBDS_HELLO_SRC)
	rgblink -o $(RGBDS_HELLO_ROM) $(RGBDS_HELLO_OBJ)
	rgbfix -v -p 0xFF $(RGBDS_HELLO_ROM)
	./$(TARGET) $(RGBDS_HELLO_ROM) 2>&1 | grep -q "HELLO GAMEBOY" \
		&& echo "gameboy-rgbds-test: OK (RGBDS-built ROM ran correctly)" \
		|| (echo "gameboy-rgbds-test: FAIL (expected serial output not seen)"; exit 1)

gameboy-rgbds-mbc3-test: $(TARGET) | $(BIN_DIR)
	rgbasm -o $(RGBDS_MBC3_RTC_OBJ) $(RGBDS_MBC3_RTC_SRC)
	rgblink -o $(RGBDS_MBC3_RTC_ROM) $(RGBDS_MBC3_RTC_OBJ)
	rgbfix -v -m 0x10 -r 0x03 -p 0xFF $(RGBDS_MBC3_RTC_ROM)
	./$(TARGET) $(RGBDS_MBC3_RTC_ROM) 2>&1 | \
		grep -q "RAM:Rr RTC1:ABCDE RTC2(unlatched):ABCDE RTC3(relatched):abcde DONE" \
		&& echo "gameboy-rgbds-mbc3-test: OK (RTC latch/isolation behavior correct)" \
		|| (echo "gameboy-rgbds-mbc3-test: FAIL (expected serial output not seen)"; exit 1)

gameboy-rgbds-hdma-test: $(TARGET) | $(BIN_DIR)
	rgbasm -o $(RGBDS_HDMA_OBJ) $(RGBDS_HDMA_SRC)
	rgblink -o $(RGBDS_HDMA_ROM) $(RGBDS_HDMA_OBJ)
	rgbfix -v -p 0xFF $(RGBDS_HDMA_ROM)
	./$(TARGET) $(RGBDS_HDMA_ROM) --mode cgb 2>&1 | \
		grep -q " R1:GDMAROUND1BYTES! R2:HBLANKROUND2BYTES0123456789ABCDE R3a:BANK1-ISOLATION! R3b:GDMAROUND1BYTES! DONE" \
		&& echo "gameboy-rgbds-hdma-test: OK (GDMA/HBlank-DMA/VRAM-bank-isolation all correct through real CPU+PPU timing)" \
		|| (echo "gameboy-rgbds-hdma-test: FAIL (expected serial output not seen)"; exit 1)

gameboy-prism-build: $(TARGET) | $(BIN_DIR)
	$(MAKE) -C prism
	rm -f $(PRISM_SAV_OUT)
	./$(TARGET) $(PRISM_ROM) --mode cgb --input $(PRISM_SCRIPT) --sav $(PRISM_SAV_OUT) --ppm $(PRISM_OUT) --frames 200
	cmp $(PRISM_OUT) $(PRISM_REF) \
		&& echo "gameboy-prism-build: OK (Milestone 7/8 - title screen -> Start -> a reverted swap slides back apart, then a real match slides together and clears via a flash+shrink animation)" \
		|| (echo "gameboy-prism-build: FAIL (rendered frame doesn't match $(PRISM_REF))"; exit 1)
	./$(TARGET) $(PRISM_ROM) --mode cgb --input $(PRISM_SCRIPT) --wav $(PRISM_WAV_OUT) --seconds 3
	cmp $(PRISM_WAV_OUT) $(PRISM_WAV_REF) \
		&& echo "gameboy-prism-build: OK (Milestone 6a - select/revert/match sound effects match a real captured reference)" \
		|| (echo "gameboy-prism-build: FAIL (captured audio doesn't match $(PRISM_WAV_REF))"; exit 1)
	cmp $(PRISM_SAV_OUT) $(PRISM_SAV_REF) \
		&& echo "gameboy-prism-build: OK (Milestone 6c - a real match-clearing swap persists a new high score to cart RAM)" \
		|| (echo "gameboy-prism-build: FAIL (saved cart RAM doesn't match $(PRISM_SAV_REF))"; exit 1)
	./$(TARGET) $(PRISM_ROM) --mode cgb --sav $(PRISM_SAV_OUT) --ppm $(PRISM_TITLE_OUT) --frames 15
	cmp $(PRISM_TITLE_OUT) $(PRISM_TITLE_REF) \
		&& echo "gameboy-prism-build: OK (Milestone 6c - a fresh boot loads the persisted high score onto the title screen)" \
		|| (echo "gameboy-prism-build: FAIL (title screen doesn't match $(PRISM_TITLE_REF))"; exit 1)

gameboy-wayfarer-build: $(TARGET) | $(BIN_DIR)
	$(MAKE) -C wayfarer
	rm -f $(WAYFARER_SAV_OUT)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_SCRIPT) --sav $(WAYFARER_SAV_OUT) --ppm $(WAYFARER_OUT) --frames 475
	cmp $(WAYFARER_OUT) $(WAYFARER_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 11 - after winning, Start wipes the save and restarts a fresh session)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_SCRIPT) --wav $(WAYFARER_WAV_OUT) --seconds 9
	cmp $(WAYFARER_WAV_OUT) $(WAYFARER_WAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 7 - swing/hit/damage/win sound effects match a real captured reference)" \
		|| (echo "gameboy-wayfarer-build: FAIL (captured audio doesn't match $(WAYFARER_WAV_REF))"; exit 1)
	cmp $(WAYFARER_SAV_OUT) $(WAYFARER_SAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 11 - restarting after a win wipes cart RAM back to a genuinely fresh save)" \
		|| (echo "gameboy-wayfarer-build: FAIL (saved cart RAM doesn't match $(WAYFARER_SAV_REF))"; exit 1)
	rm -f $(WAYFARER_WON_SAV_OUT)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_WON_SAV_SCRIPT) --sav $(WAYFARER_WON_SAV_OUT) --frames 441
	cmp $(WAYFARER_WON_SAV_OUT) $(WAYFARER_WON_SAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 9 - defeating the enemy, collecting the key, and winning persists to cart RAM)" \
		|| (echo "gameboy-wayfarer-build: FAIL (saved cart RAM doesn't match $(WAYFARER_WON_SAV_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --sav $(WAYFARER_WON_SAV_OUT) --input $(WAYFARER_WON_SCRIPT) --ppm $(WAYFARER_WON_OUT) --frames 15
	cmp $(WAYFARER_WON_OUT) $(WAYFARER_WON_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 9 - a fresh boot loading a won save shows the win screen immediately after the title)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_WON_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_BRUTE_SCRIPT) --ppm $(WAYFARER_BRUTE_ALIVE_OUT) --frames 795
	cmp $(WAYFARER_BRUTE_ALIVE_OUT) $(WAYFARER_BRUTE_ALIVE_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 12 - the brute's own 4 quadrant tiles render correctly while alive, no tile ID collision)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_BRUTE_ALIVE_REF))"; exit 1)
	rm -f $(WAYFARER_BRUTE_SAV_OUT)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_BRUTE_SCRIPT) --sav $(WAYFARER_BRUTE_SAV_OUT) --ppm $(WAYFARER_BRUTE_OUT) --frames 905
	cmp $(WAYFARER_BRUTE_OUT) $(WAYFARER_BRUTE_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 12 - the brute takes two hits to die, sitting in optional room (2,1))" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_BRUTE_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_BRUTE_SCRIPT) --wav $(WAYFARER_BRUTE_WAV_OUT) --seconds 19
	cmp $(WAYFARER_BRUTE_WAV_OUT) $(WAYFARER_BRUTE_WAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 12 - the new channel-2 brute-hit thud on hit 1, the existing hit sfx on hit 2)" \
		|| (echo "gameboy-wayfarer-build: FAIL (captured audio doesn't match $(WAYFARER_BRUTE_WAV_REF))"; exit 1)
	cmp $(WAYFARER_BRUTE_SAV_OUT) $(WAYFARER_BRUTE_SAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 12 - defeating the brute persists BIT_BRUTE to cart RAM)" \
		|| (echo "gameboy-wayfarer-build: FAIL (saved cart RAM doesn't match $(WAYFARER_BRUTE_SAV_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_SHIELD_SCRIPT) --ppm $(WAYFARER_SHIELD_BLOCKED_OUT) --frames 625
	cmp $(WAYFARER_SHIELD_BLOCKED_OUT) $(WAYFARER_SHIELD_BLOCKED_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 14 - the shield blocks a contact hit while facing the brute)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_SHIELD_BLOCKED_REF))"; exit 1)
	rm -f $(WAYFARER_SHIELD_SAV_OUT)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_SHIELD_SCRIPT) --sav $(WAYFARER_SHIELD_SAV_OUT) --ppm $(WAYFARER_SHIELD_OUT) --frames 700
	cmp $(WAYFARER_SHIELD_OUT) $(WAYFARER_SHIELD_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 14 - the block correctly stops once the brute's patrol carries it out of the faced direction)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_SHIELD_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_SHIELD_SCRIPT) --wav $(WAYFARER_SHIELD_WAV_OUT) --seconds 13
	cmp $(WAYFARER_SHIELD_WAV_OUT) $(WAYFARER_SHIELD_WAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 14 - the block sfx fires once, then the existing damage sfx once the block ends)" \
		|| (echo "gameboy-wayfarer-build: FAIL (captured audio doesn't match $(WAYFARER_SHIELD_WAV_REF))"; exit 1)
	cmp $(WAYFARER_SHIELD_SAV_OUT) $(WAYFARER_SHIELD_SAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 14 - collecting the shield persists BIT_SHIELD to cart RAM)" \
		|| (echo "gameboy-wayfarer-build: FAIL (saved cart RAM doesn't match $(WAYFARER_SHIELD_SAV_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_MUSIC_SCRIPT) --wav $(WAYFARER_MUSIC_WAV_OUT) --seconds 10
	cmp $(WAYFARER_MUSIC_WAV_OUT) $(WAYFARER_MUSIC_WAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 15 - the background theme's melody, rest, and loop restart match a real captured reference)" \
		|| (echo "gameboy-wayfarer-build: FAIL (captured audio doesn't match $(WAYFARER_MUSIC_WAV_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_BOSS_SCRIPT) --ppm $(WAYFARER_BOSS_ALIVE_OUT) --frames 990
	cmp $(WAYFARER_BOSS_ALIVE_OUT) $(WAYFARER_BOSS_ALIVE_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 16 - the boss's own 9 quadrant tiles render correctly while alive, reusing the brute's palette safely)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_BOSS_ALIVE_REF))"; exit 1)
	rm -f $(WAYFARER_BOSS_SAV_OUT)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_BOSS_SCRIPT) --sav $(WAYFARER_BOSS_SAV_OUT) --ppm $(WAYFARER_BOSS_OUT) --frames 1180
	cmp $(WAYFARER_BOSS_OUT) $(WAYFARER_BOSS_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 16 - the boss takes three hits to die, sitting in the new optional room (2,2))" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_BOSS_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_BOSS_SCRIPT) --wav $(WAYFARER_BOSS_WAV_OUT) --seconds 21
	cmp $(WAYFARER_BOSS_WAV_OUT) $(WAYFARER_BOSS_WAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 16 - the brute-hit thud on hits 1-2, the win sound on the third, lethal hit)" \
		|| (echo "gameboy-wayfarer-build: FAIL (captured audio doesn't match $(WAYFARER_BOSS_WAV_REF))"; exit 1)
	cmp $(WAYFARER_BOSS_SAV_OUT) $(WAYFARER_BOSS_SAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 16 - defeating the boss persists BIT_BOSS to cart RAM)" \
		|| (echo "gameboy-wayfarer-build: FAIL (saved cart RAM doesn't match $(WAYFARER_BOSS_SAV_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_CHEST_SCRIPT) --ppm $(WAYFARER_CHEST_COLLECTED_OUT) --frames 615
	cmp $(WAYFARER_CHEST_COLLECTED_OUT) $(WAYFARER_CHEST_COLLECTED_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 17 - collecting the chest grants a 4th, full heart)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_CHEST_COLLECTED_REF))"; exit 1)
	rm -f $(WAYFARER_CHEST_SAV_OUT)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_CHEST_SCRIPT) --sav $(WAYFARER_CHEST_SAV_OUT) --ppm $(WAYFARER_CHEST_HIT_OUT) --frames 780
	cmp $(WAYFARER_CHEST_HIT_OUT) $(WAYFARER_CHEST_HIT_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 17 - heart_hud.c's generalized HUD renders partial damage correctly at the new 4-heart width)" \
		|| (echo "gameboy-wayfarer-build: FAIL (rendered frame doesn't match $(WAYFARER_CHEST_HIT_REF))"; exit 1)
	./$(TARGET) $(WAYFARER_ROM) --mode cgb --input $(WAYFARER_CHEST_SCRIPT) --wav $(WAYFARER_CHEST_WAV_OUT) --seconds 15
	cmp $(WAYFARER_CHEST_WAV_OUT) $(WAYFARER_CHEST_WAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 17 - the shared pickup chime on collection, the existing damage sfx on the later graze)" \
		|| (echo "gameboy-wayfarer-build: FAIL (captured audio doesn't match $(WAYFARER_CHEST_WAV_REF))"; exit 1)
	cmp $(WAYFARER_CHEST_SAV_OUT) $(WAYFARER_CHEST_SAV_REF) \
		&& echo "gameboy-wayfarer-build: OK (Milestone 17 - collecting the chest persists BIT_CHEST to the new second SRAM state byte)" \
		|| (echo "gameboy-wayfarer-build: FAIL (saved cart RAM doesn't match $(WAYFARER_CHEST_SAV_REF))"; exit 1)

gameboy-mooneye-test: $(TARGET)
	python3 tests/run_mooneye.py $(TARGET) $(MOONEYE_DIR)

gameboy-sdl: $(SDL_TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(SDL_TARGET): $(SDL_OBJS) $(CORE_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SDL_OBJS) $(CORE_OBJS) $(SDL_LIBS)

$(TEST_TARGET): tests/test_cart.c $(SRC_DIR)/cart.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ tests/test_cart.c $(SRC_DIR)/cart.c

$(TEST_TIMER_TARGET): tests/test_timer.c $(SRC_DIR)/timer.c $(SRC_DIR)/mmu.c $(SRC_DIR)/cart.c $(SRC_DIR)/ppu.c $(SRC_DIR)/joypad.c $(SRC_DIR)/apu.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -lm -o $@ tests/test_timer.c $(SRC_DIR)/timer.c $(SRC_DIR)/mmu.c $(SRC_DIR)/cart.c $(SRC_DIR)/ppu.c $(SRC_DIR)/joypad.c $(SRC_DIR)/apu.c

$(TEST_APU_TARGET): tests/test_apu.c $(SRC_DIR)/apu.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -lm -o $@ tests/test_apu.c $(SRC_DIR)/apu.c

$(TEST_CPU_TARGET): tests/test_cpu.c $(SRC_DIR)/cpu.c $(SRC_DIR)/alu.c $(SRC_DIR)/mmu.c $(SRC_DIR)/cart.c $(SRC_DIR)/ppu.c $(SRC_DIR)/joypad.c $(SRC_DIR)/apu.c $(SRC_DIR)/timer.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -lm -o $@ tests/test_cpu.c $(SRC_DIR)/cpu.c $(SRC_DIR)/alu.c $(SRC_DIR)/mmu.c $(SRC_DIR)/cart.c $(SRC_DIR)/ppu.c $(SRC_DIR)/joypad.c $(SRC_DIR)/apu.c $(SRC_DIR)/timer.c

$(TEST_SAVESTATE_TARGET): tests/test_savestate.c $(SRC_DIR)/savestate.c $(SRC_DIR)/cpu.c $(SRC_DIR)/alu.c $(SRC_DIR)/mmu.c $(SRC_DIR)/cart.c $(SRC_DIR)/ppu.c $(SRC_DIR)/joypad.c $(SRC_DIR)/apu.c $(SRC_DIR)/timer.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -lm -o $@ tests/test_savestate.c $(SRC_DIR)/savestate.c $(SRC_DIR)/cpu.c $(SRC_DIR)/alu.c $(SRC_DIR)/mmu.c $(SRC_DIR)/cart.c $(SRC_DIR)/ppu.c $(SRC_DIR)/joypad.c $(SRC_DIR)/apu.c $(SRC_DIR)/timer.c

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SDL_SRC_DIR)/%.o: $(SDL_SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(SDL_OBJS) $(TARGET) $(TEST_TARGET) $(TEST_TIMER_TARGET) $(TEST_APU_TARGET) $(TEST_CPU_TARGET) $(TEST_SAVESTATE_TARGET) $(VISUAL_OUT) $(CGB_VISUAL_OUT) $(GB2048_OUT) $(DRONEBOY_OUT) $(TOBU_OUT) $(SAVESTATE_CONTINUOUS) $(SAVESTATE_MID_PPM) $(SAVESTATE_MID_STATE) $(SAVESTATE_RESUMED) $(SDL_TARGET) $(RGBDS_HELLO_OBJ) $(RGBDS_HELLO_ROM) $(RGBDS_MBC3_RTC_OBJ) $(RGBDS_MBC3_RTC_ROM) $(RGBDS_HDMA_OBJ) $(RGBDS_HDMA_ROM) $(PRISM_OUT) $(PRISM_WAV_OUT) $(PRISM_SAV_OUT) $(PRISM_TITLE_OUT) $(WAYFARER_OUT) $(WAYFARER_WAV_OUT) $(WAYFARER_SAV_OUT) $(WAYFARER_WON_SAV_OUT) $(WAYFARER_WON_OUT) $(WAYFARER_BRUTE_OUT) $(WAYFARER_BRUTE_WAV_OUT) $(WAYFARER_BRUTE_SAV_OUT) $(WAYFARER_BRUTE_ALIVE_OUT) $(WAYFARER_SHIELD_OUT) $(WAYFARER_SHIELD_BLOCKED_OUT) $(WAYFARER_SHIELD_WAV_OUT) $(WAYFARER_SHIELD_SAV_OUT) $(WAYFARER_MUSIC_WAV_OUT) $(WAYFARER_BOSS_ALIVE_OUT) $(WAYFARER_BOSS_OUT) $(WAYFARER_BOSS_WAV_OUT) $(WAYFARER_BOSS_SAV_OUT) $(WAYFARER_CHEST_COLLECTED_OUT) $(WAYFARER_CHEST_HIT_OUT) $(WAYFARER_CHEST_WAV_OUT) $(WAYFARER_CHEST_SAV_OUT)
	$(MAKE) -C prism clean
	$(MAKE) -C wayfarer clean
