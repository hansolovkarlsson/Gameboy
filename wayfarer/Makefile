# "Wayfarer" (working title) - a new, separate homebrew game from the
# sibling prism/ project. See wayfarer/README.md for background and
# docs/GAMEBOY_ROADMAP.md for the milestone roadmap this Makefile's own
# gb target is Milestone 1 of.
#
# GBDK_HOME defaults to the toolchain already extracted for prism/
# (../prism/toolchain/gbdk, gitignored there) rather than fetching a
# second copy of the same multi-hundred-MB release archive - see
# wayfarer/README.md. Override GBDK_HOME if you want an independent
# install.

GBDK_HOME ?= ../prism/toolchain/gbdk
LCC := $(GBDK_HOME)/bin/lcc

SRC_DIR := src
BIN_DIR := bin
BINS := $(BIN_DIR)/wayfarer.gb

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%.o,$(SRCS))

# -Wm-yC: CGB-only cartridge header flag (0x0143 = $C0), matching
# prism/Makefile's own use of the same flag - this project's own
# gb_resolve_cgb_mode() (src/cart.c) would run it in CGB mode
# regardless via --mode cgb, but the ROM's own header should tell the
# truth too.
# -Wl-yt0x03: cartridge type MBC1+RAM+BATTERY (confirmed against this
# project's own src/cart.c header-parsing switch, already proven
# working there and by test_roms/2048-gb's own real MBC1+battery ROM) -
# Milestone 9's own SRAM save (src/sram.c) needs real, persistent
# cartridge RAM.
# -Wm-ya1: 1 cart RAM bank (8 KiB) - GBDK's smallest selectable unit,
# far more than the 2 bytes sram.c actually needs. Same flags
# prism/Makefile already uses for its own highscore.c.
LCCFLAGS := -Wm-yC -Wl-yt0x03 -Wm-ya1

all: $(BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(LCC) $(LCCFLAGS) -c -o $@ $<

$(BIN_DIR)/wayfarer.gb: $(OBJS) | $(BIN_DIR)
	$(LCC) $(LCCFLAGS) -o $@ $(OBJS)

clean:
	rm -rf $(BIN_DIR) *.lst *.map *.ihx *.sym *.cdb *.adb *.asm *.noi *.rst

.PHONY: all clean
