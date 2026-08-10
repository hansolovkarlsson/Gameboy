# "Prism" (working title) - this project's first original homebrew
# game. See prism/README.md for the toolchain install steps and
# licensing note, and docs/GAMEBOY_ROADMAP.md for the milestone
# roadmap this Makefile's own gb target is Milestone 1 of.
#
# GBDK_HOME is not vendored in this repo (see prism/README.md) -
# extract the official gbdk-macos-arm64.tar.gz release archive to
# prism/toolchain/gbdk/ (gitignored), matching the layout this Makefile
# expects by default. Override GBDK_HOME to point elsewhere if you've
# installed it somewhere else.

GBDK_HOME ?= toolchain/gbdk
LCC := $(GBDK_HOME)/bin/lcc

SRC_DIR := src
BIN_DIR := bin
BINS := $(BIN_DIR)/prism.gb

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%.o,$(SRCS))

# -Wm-yC: CGB-only cartridge header flag (0x0143 = $C0) - this project's
# own gb_resolve_cgb_mode() (src/cart.c) would run it in CGB mode
# regardless via --mode cgb, but the ROM's own header should tell the
# truth too, matching how a real CGB-only cart's header actually reads.
# -Wl-yt0x03: cartridge type MBC1+RAM+BATTERY (confirmed against this
# project's own src/cart.c header-parsing switch) - the high score
# (prism/src/highscore.c) needs real, persistent cartridge RAM.
# -Wm-ya1: 1 cart RAM bank (8 KiB) - GBDK's smallest selectable unit,
# far more than the 3 bytes highscore.c actually needs.
LCCFLAGS := -Wm-yC -Wl-yt0x03 -Wm-ya1

all: $(BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/gems.h | $(BIN_DIR)
	$(LCC) $(LCCFLAGS) -c -o $@ $<

$(BIN_DIR)/prism.gb: $(OBJS) | $(BIN_DIR)
	$(LCC) $(LCCFLAGS) -o $@ $(OBJS)

clean:
	rm -rf $(BIN_DIR) *.lst *.map *.ihx *.sym *.cdb *.adb *.asm *.noi *.rst

.PHONY: all clean
