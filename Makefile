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

# The real GTK4+Cairo+CoreAudio front end (gtk/src/main.c) - opt-in, the
# only build target with an external dependency beyond a bare C
# compiler, and links the core directly instead of spawning a separate
# process (see gtk/src/main.c's own top comment for why the sibling
# Z80/CP-M repo's cpm/gtk's spawn-and-hand-a-pty-to-VTE approach doesn't
# transfer here). Built from the core sources directly rather than
# $(OBJS), since that includes src/main.c's own competing main(). Only
# `gtk4` is needed (not a VTE package) - gtk4's own pkg-config Requires
# already pulls in Cairo's include path, and this front end never uses
# VTE at all.
CORE_SRCS := $(filter-out $(SRC_DIR)/main.c,$(SRCS))
CORE_OBJS := $(CORE_SRCS:.c=.o)
GTK_SRC_DIR := gtk/src
GTK_SRCS := $(wildcard $(GTK_SRC_DIR)/*.c)
GTK_OBJS := $(GTK_SRCS:.c=.o)
GTK_TARGET := $(BIN_DIR)/gameboy-gtk
GTK_PKGS := gtk4
GTK_CFLAGS := $(shell pkg-config --cflags $(GTK_PKGS) 2>/dev/null) -I$(SRC_DIR)
GTK_LIBS := $(shell pkg-config --libs $(GTK_PKGS) 2>/dev/null)
# -framework AudioToolbox: live audio via CoreAudio's AudioQueue (see
# gtk/src/main.c's own comment for why CoreAudio specifically, not a
# portable library) - a macOS system framework, no brew/pkg-config
# dependency needed.
GTK_LIBS += -framework AudioToolbox

.PHONY: all gameboy gameboy-test gameboy-visual-test gameboy-2048-test gameboy-droneboy-test gameboy-tobu-test gameboy-rgbds-test gameboy-rgbds-mbc3-test gameboy-savestate-test gameboy-mooneye-test gameboy-gtk clean

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

gameboy-mooneye-test: $(TARGET)
	python3 tests/run_mooneye.py $(TARGET) $(MOONEYE_DIR)

gameboy-gtk: $(GTK_TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(GTK_TARGET): $(GTK_OBJS) $(CORE_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(GTK_OBJS) $(CORE_OBJS) $(GTK_LIBS)

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

$(GTK_SRC_DIR)/%.o: $(GTK_SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(GTK_OBJS) $(TARGET) $(TEST_TARGET) $(TEST_TIMER_TARGET) $(TEST_APU_TARGET) $(TEST_CPU_TARGET) $(TEST_SAVESTATE_TARGET) $(VISUAL_OUT) $(GB2048_OUT) $(DRONEBOY_OUT) $(TOBU_OUT) $(SAVESTATE_CONTINUOUS) $(SAVESTATE_MID_PPM) $(SAVESTATE_MID_STATE) $(SAVESTATE_RESUMED) $(GTK_TARGET) $(RGBDS_HELLO_OBJ) $(RGBDS_HELLO_ROM) $(RGBDS_MBC3_RTC_OBJ) $(RGBDS_MBC3_RTC_ROM)
