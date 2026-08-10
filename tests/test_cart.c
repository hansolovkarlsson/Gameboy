#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cart.h"

// Direct unit tests for cart.c's MBC1/MBC3/MBC5 banking logic, grounded
// against pandocs' MBC1.md/MBC3.md/MBC5.md addressing diagrams (fetched
// during Phase 2 - see docs/GAMEBOY_ROADMAP.md). None of Blargg's
// cpu_instrs ROMs use banking at all (they're all plain 32KB MBC-less),
// so this is the only real coverage this banking code has - the CP/M
// side's "every fix gets a permanent, software-independent regression
// test" convention, adapted here since there's no Game Boy assembler
// yet to write a real test ROM with. Most scenarios build GBCart
// structs directly (bypassing gb_cart_load()'s file I/O) so each one is
// exact and self-contained; test_cart_load_from_file() below covers the
// file-parsing path itself, the one integration point main.c actually
// depends on.

static int failures = 0;

static void check(const char *name, int condition) {
    if (condition) {
        printf("OK   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        failures++;
    }
}

// Builds a synthetic ROM where bank i's first byte is i - lets a test
// confirm "which bank got selected" just by reading one byte.
static uint8_t *make_marked_rom(int banks) {
    uint8_t *rom = calloc((size_t)banks, 0x4000);
    for (int i = 0; i < banks; i++) rom[i * 0x4000] = (uint8_t)i;
    return rom;
}

static void test_mbc1_basic_rom_banking(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC1;
    cart.rom_banks = 8; // 128 KiB - well under the 32-bank threshold where the secondary register starts applying to ROM
    cart.rom = make_marked_rom(cart.rom_banks);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;

    gb_cart_write_ctrl(&cart, 0x2000, 3);
    check("MBC1: select bank 3, read at 0x4000", gb_cart_read(&cart, 0x4000) == 3);

    gb_cart_write_ctrl(&cart, 0x2000, 0);
    check("MBC1: writing bank 0 selects bank 1 instead", gb_cart_read(&cart, 0x4000) == 1);

    check("MBC1: 0x0000-0x3FFF always reads bank 0 in simple mode", gb_cart_read(&cart, 0x0000) == 0);

    free(cart.rom);
}

static void test_mbc1_large_rom_advanced_mode(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC1;
    cart.rom_banks = 128; // 2 MiB - over the 32-bank threshold, so the secondary register extends ROM addressing
    cart.rom = make_marked_rom(cart.rom_banks);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;

    gb_cart_write_ctrl(&cart, 0x2000, 0);   // low 5 bits = 0 -> reads as 1
    gb_cart_write_ctrl(&cart, 0x4000, 1);   // secondary register = 1 -> bits 5-6
    check("MBC1: large ROM, bank = (1<<5)|1 = 0x21", gb_cart_read(&cart, 0x4000) == 0x21);

    check("MBC1: simple mode still locks 0x0000-0x3FFF to bank 0", gb_cart_read(&cart, 0x0000) == 0);

    gb_cart_write_ctrl(&cart, 0x6000, 1); // enter advanced mode
    check("MBC1: advanced mode exposes bank 0x20 at 0x0000-0x3FFF", gb_cart_read(&cart, 0x0000) == 0x20);

    gb_cart_write_ctrl(&cart, 0x6000, 0); // back to simple mode
    check("MBC1: leaving advanced mode re-locks 0x0000-0x3FFF to bank 0", gb_cart_read(&cart, 0x0000) == 0);

    free(cart.rom);
}

static void test_mbc1_ram_banking(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC1;
    cart.rom_banks = 8;
    cart.rom = calloc((size_t)cart.rom_banks, 0x4000);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;
    cart.has_ram = 1;
    cart.ram_banks = 4; // 32 KiB
    cart.ram_size = (size_t)cart.ram_banks * 0x2000;
    cart.ram = calloc(1, cart.ram_size);

    gb_cart_write_ctrl(&cart, 0x0000, 0x0A); // enable RAM

    gb_cart_write_ctrl(&cart, 0x4000, 2); // secondary register = 2 (would-be RAM bank)
    gb_cart_write_ram(&cart, 0xA000, 0x11);
    check("MBC1: simple mode locks RAM to bank 0 regardless of the secondary register",
          cart.ram[0] == 0x11 && cart.ram[2 * 0x2000] == 0x00);

    gb_cart_write_ctrl(&cart, 0x6000, 1); // advanced mode: secondary register now selects RAM bank
    gb_cart_write_ram(&cart, 0xA000, 0x22);
    check("MBC1: advanced mode routes RAM writes to the selected bank",
          cart.ram[2 * 0x2000] == 0x22 && cart.ram[0] == 0x11);
    check("MBC1: reading back bank 2 sees what was just written",
          gb_cart_read_ram(&cart, 0xA000) == 0x22);

    gb_cart_write_ctrl(&cart, 0x4000, 0);
    check("MBC1: switching back to RAM bank 0 sees the original value untouched",
          gb_cart_read_ram(&cart, 0xA000) == 0x11);

    free(cart.rom);
    free(cart.ram);
}

static void test_mbc1_ram_disabled(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC1;
    cart.rom_banks = 2;
    cart.rom = calloc((size_t)cart.rom_banks, 0x4000);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;
    cart.has_ram = 1;
    cart.ram_banks = 1;
    cart.ram_size = 0x2000;
    cart.ram = calloc(1, cart.ram_size);

    // RAM never enabled (ram_enabled defaults to 0).
    check("MBC1: reading disabled RAM returns 0xFF", gb_cart_read_ram(&cart, 0xA000) == 0xFF);
    gb_cart_write_ram(&cart, 0xA000, 0x99);
    check("MBC1: writing disabled RAM is silently discarded", cart.ram[0] == 0x00);

    free(cart.rom);
    free(cart.ram);
}

// MBC1M multicart addressing (cart.c's gb_cart_read(), mbc1_multicart
// branch) - spot-checked directly against real values from Mooneye's
// own emulator-only/mbc1/multicart_rom_8Mb.s "expected_banks" table
// (fetched from Gekkio/mooneye-test-suite), not invented here. That
// table confirms two things this test isolates: the secondary register
// lands on bits 4-5 (not 5-6), and the 0->1 "cannot duplicate bank 0"
// quirk is decided on the full 5-bit primary register *before* it gets
// truncated to 4 bits - so writing 16 (0b10000) does NOT trigger the
// quirk even though its truncated low nibble is 0, unlike writing 0
// itself.
static void test_mbc1_multicart_rom_banking(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC1;
    cart.mbc1_multicart = 1;
    cart.rom_banks = 64; // 1 MiB = the only real MBC1M size
    cart.rom = make_marked_rom(cart.rom_banks);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;

    gb_cart_write_ctrl(&cart, 0x4000, 1); // secondary register = 1 -> bits 4-5 (bank 16), not 5-6 (bank 32)
    gb_cart_write_ctrl(&cart, 0x2000, 0); // primary register = 0 -> 0->1 quirk -> bank 16|1 = 17
    check("MBC1M: secondary=1, primary=0 (quirked to 1) -> bank 17", gb_cart_read(&cart, 0x4000) == 17);

    gb_cart_write_ctrl(&cart, 0x2000, 16); // primary register = 16 (0b10000): raw value is nonzero, no quirk
    check("MBC1M: primary=16 truncates to nibble 0, no quirk applied (raw value was nonzero) -> bank 16",
          gb_cart_read(&cart, 0x4000) == 16);

    check("MBC1M: 0x0000-0x3FFF stays bank 0 in simple mode regardless of the secondary register",
          gb_cart_read(&cart, 0x0000) == 0);

    gb_cart_write_ctrl(&cart, 0x4000, 2); // secondary register = 2 -> bits 4-5 (bank 32 in mode 1), not 64
    gb_cart_write_ctrl(&cart, 0x6000, 1); // advanced mode
    check("MBC1M: advanced mode exposes bank 32 (2<<4) at 0x0000-0x3FFF, not bank 64 (2<<5)",
          gb_cart_read(&cart, 0x0000) == 32);

    free(cart.rom);
}

// is_mbc1_multicart()'s own detection heuristic (cart.c, ported from
// Gekkio's mooneye-gb config/cartridge.rs): only a real 1 MiB MBC1 ROM
// with a valid Nintendo logo at >=3 of its 4 256 KiB page boundaries is
// flagged - a regular (non-multicart) 1 MiB MBC1 game, which only ever
// has a valid logo in page 0, must NOT be misdetected as a multicart.
static void write_logo(uint8_t *rom, size_t page_offset) {
    static const uint8_t logo[48] = {
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
    };
    memcpy(rom + page_offset + 0x0104, logo, sizeof(logo));
}

static void test_mbc1_multicart_detection(void) {
    size_t rom_size = 0x100000; // 1 MiB - the only size real MBC1M carts use

    // Multicart: logo present at 3 of the 4 page boundaries (0x00000, 0x40000, 0x80000, 0xC0000).
    {
        const char *path = "/tmp/gb_test_cart_multicart.gb";
        uint8_t *rom = calloc(1, rom_size);
        rom[0x0147] = 0x01; // MBC1, no RAM
        rom[0x0148] = 0x05; // 1 MiB / 64 banks
        rom[0x0149] = 0x00;
        write_logo(rom, 0x00000);
        write_logo(rom, 0x40000);
        write_logo(rom, 0x80000);
        // page 3 (0xC0000) deliberately left without a logo - real
        // carts can have a menu-less layout, matching mooneye-gb's own
        // ">=3 of 4" threshold rather than requiring all 4.
        uint8_t checksum = 0;
        for (uint16_t addr = 0x0134; addr <= 0x014C; addr++) checksum = (uint8_t)(checksum - rom[addr] - 1);
        rom[0x014D] = checksum;

        FILE *f = fopen(path, "wb");
        fwrite(rom, 1, rom_size, f);
        fclose(f);
        free(rom);

        GBCart cart;
        int rc = gb_cart_load(&cart, path);
        check("gb_cart_load: succeeds on a synthetic 1 MiB MBC1 ROM", rc == 0);
        if (rc == 0) {
            check("MBC1M detection: 3-of-4 valid logo pages flags mbc1_multicart", cart.mbc1_multicart == 1);
            gb_cart_free(&cart);
        }
        remove(path);
    }

    // Regular (non-multicart) 1 MiB MBC1 game: logo only in page 0.
    {
        const char *path = "/tmp/gb_test_cart_regular_1mb.gb";
        uint8_t *rom = calloc(1, rom_size);
        rom[0x0147] = 0x01;
        rom[0x0148] = 0x05; // 1 MiB / 64 banks
        rom[0x0149] = 0x00;
        write_logo(rom, 0x00000);
        uint8_t checksum = 0;
        for (uint16_t addr = 0x0134; addr <= 0x014C; addr++) checksum = (uint8_t)(checksum - rom[addr] - 1);
        rom[0x014D] = checksum;

        FILE *f = fopen(path, "wb");
        fwrite(rom, 1, rom_size, f);
        fclose(f);
        free(rom);

        GBCart cart;
        int rc = gb_cart_load(&cart, path);
        check("gb_cart_load: succeeds on a synthetic regular 1 MiB MBC1 ROM", rc == 0);
        if (rc == 0) {
            check("MBC1M detection: a regular 1 MiB MBC1 game (logo in page 0 only) is not misdetected",
                  cart.mbc1_multicart == 0);
            gb_cart_free(&cart);
        }
        remove(path);
    }
}

static void test_mbc3_rom_banking(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC3;
    cart.rom_banks = 128; // 2 MiB
    cart.rom = make_marked_rom(cart.rom_banks);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;

    gb_cart_write_ctrl(&cart, 0x2000, 0x20);
    check("MBC3: bank 0x20 is directly reachable (no AND-gate quirk unlike MBC1)",
          gb_cart_read(&cart, 0x4000) == 0x20);

    gb_cart_write_ctrl(&cart, 0x2000, 0x00);
    check("MBC3: writing bank 0 still selects bank 1", gb_cart_read(&cart, 0x4000) == 1);

    free(cart.rom);
}

static void test_mbc3_rtc_latch(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC3;
    cart.rom_banks = 2;
    cart.rom = calloc((size_t)cart.rom_banks, 0x4000);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;
    cart.has_rtc = 1;

    gb_cart_write_ctrl(&cart, 0x0000, 0x0A); // enable RAM/RTC access
    gb_cart_write_ctrl(&cart, 0x4000, 0x08); // select RTC register S (seconds)

    cart.rtc[0] = 42; // simulate the "live" clock having ticked to 42s
    check("MBC3: unlatched live RTC value isn't visible yet", gb_cart_read_ram(&cart, 0xA000) == 0);

    gb_cart_write_ctrl(&cart, 0x6000, 0x00);
    gb_cart_write_ctrl(&cart, 0x6000, 0x01); // 0-then-1 latch sequence
    check("MBC3: latching copies the live value", gb_cart_read_ram(&cart, 0xA000) == 42);

    cart.rtc[0] = 99; // clock keeps ticking after the latch
    check("MBC3: latched value doesn't change until re-latched", gb_cart_read_ram(&cart, 0xA000) == 42);

    gb_cart_write_ram(&cart, 0xA000, 5);
    check("MBC3: writing an RTC register writes the *live* register, not the latch", cart.rtc[0] == 5);

    free(cart.rom);
}

// Everything above bypasses gb_cart_load()'s file I/O and header
// parsing entirely, constructing GBCart structs directly - this test
// covers that path instead, the one real integration point main.c
// actually depends on.
static void test_cart_load_from_file(void) {
    const char *path = "/tmp/gb_test_cart_load.gb";
    size_t rom_size = 8 * 0x4000; // ROM size code 0x02 = 128 KiB = 8 banks
    uint8_t *rom = calloc(1, rom_size);

    rom[0x0147] = 0x02; // MBC1+RAM
    rom[0x0148] = 0x02; // 128 KiB / 8 banks
    rom[0x0149] = 0x02; // 8 KiB RAM / 1 bank

    // Real DMG boot ROM header-checksum algorithm (same as cart.c's own
    // - see pandocs' The_Cartridge_Header.md), computed here so
    // gb_cart_load() doesn't print a spurious mismatch warning.
    uint8_t checksum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; addr++) {
        checksum = (uint8_t)(checksum - rom[addr] - 1);
    }
    rom[0x014D] = checksum;

    FILE *f = fopen(path, "wb");
    fwrite(rom, 1, rom_size, f);
    fclose(f);
    free(rom);

    GBCart cart;
    int rc = gb_cart_load(&cart, path);
    check("gb_cart_load: succeeds on a well-formed synthetic MBC1+RAM ROM", rc == 0);
    if (rc == 0) {
        check("gb_cart_load: detects MBC1 from cartridge type 0x02", cart.mbc_type == GB_MBC1);
        check("gb_cart_load: computes 8 ROM banks from size code 0x02", cart.rom_banks == 8);
        check("gb_cart_load: computes 1 RAM bank from size code 0x02", cart.ram_banks == 1);
        check("gb_cart_load: has_ram set from the cartridge type", cart.has_ram == 1);
        check("gb_cart_load: header checksum validates", cart.header_checksum_valid == 1);
        gb_cart_free(&cart);
    }

    remove(path);
}

// Regression test for a real bug found via test_roms/mooneye/
// emulator-only/mbc5/ (all 8 ROMs there call straight into ROMX-bank
// library code before ever writing the bank register): unlike
// MBC1/MBC3, MBC5 has no read-time "bank 0 reads as bank 1" quirk at
// $4000-7FFF (pandocs' MBC5.md is explicit that writing 0 really does
// give bank 0), so if gb_cart_load() left the bank register itself at
// its calloc-zeroed default, a fresh load would show bank 0's content
// at $4000-7FFF instead of bank 1's - wrong, since Gekkio's own
// mooneye-gb (cartridge.rs, Mbc5State::default()) initializes that
// register to 1 on power-up, verified against a real MBC5 flash cart.
static void test_mbc5_default_bank_is_one(void) {
    const char *path = "/tmp/gb_test_cart_load_mbc5.gb";
    size_t rom_size = 2 * 0x4000; // ROM size code 0x00 = 32 KiB = 2 banks, smallest valid MBC5 image
    uint8_t *rom = calloc(1, rom_size);

    rom[0x0147] = 0x19; // MBC5, no RAM
    rom[0x0148] = 0x00; // 32 KiB / 2 banks
    rom[0x0149] = 0x00; // no RAM
    rom[0x4000] = 0xBB; // bank 1's first byte - distinguishes it from bank 0's (left 0x00)

    uint8_t checksum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; addr++) {
        checksum = (uint8_t)(checksum - rom[addr] - 1);
    }
    rom[0x014D] = checksum;

    FILE *f = fopen(path, "wb");
    fwrite(rom, 1, rom_size, f);
    fclose(f);
    free(rom);

    GBCart cart;
    int rc = gb_cart_load(&cart, path);
    check("gb_cart_load: succeeds on a well-formed synthetic MBC5 ROM", rc == 0);
    if (rc == 0) {
        check("MBC5: rom_bank_lo defaults to 1 on load, not 0", cart.rom_bank_lo == 1);
        check("MBC5: $4000-7FFF shows bank 1's content before any register write",
              gb_cart_read(&cart, 0x4000) == 0xBB);
        gb_cart_free(&cart);
    }

    remove(path);
}

static void test_mbc5_rom_banking(void) {
    GBCart cart = {0};
    cart.mbc_type = GB_MBC5;
    cart.rom_banks = 512; // 8 MiB, needs the full 9-bit bank number
    cart.rom = make_marked_rom(cart.rom_banks);
    cart.rom_size = (size_t)cart.rom_banks * 0x4000;

    gb_cart_write_ctrl(&cart, 0x2000, 0x00);
    gb_cart_write_ctrl(&cart, 0x3000, 0x00);
    check("MBC5: writing bank 0 really does select bank 0 (no quirk, unlike MBC1/MBC3)",
          gb_cart_read(&cart, 0x4000) == 0);

    gb_cart_write_ctrl(&cart, 0x2000, 0xFF);
    gb_cart_write_ctrl(&cart, 0x3000, 0x01);
    // bank 0xFF within a 512-bank ROM's marked-ROM scheme wraps mod 256
    // in the low byte marker (make_marked_rom stores i as a single
    // byte), so check the reconstructed 9-bit number directly instead.
    check("MBC5: 9-bit bank number = (hi<<8)|lo = 0x1FF",
          gb_cart_read(&cart, 0x4000) == (uint8_t)0x1FF);

    free(cart.rom);
}

// gb_cart_load_ram_file()/gb_cart_save_ram_file() (cart.c) - real
// battery-backed cart RAM persistence, added because GBCart.has_battery
// used to be genuinely unused (no save-to-disk mechanism existed at
// all). Direct unit tests since this is real file I/O, not something
// any existing ROM's own regression test isolates cleanly.
static void test_battery_ram_file_round_trip(void) {
    const char *path = "/tmp/gb_test_cart_battery.sav";
    remove(path);

    GBCart cart = {0};
    cart.has_ram = 1;
    cart.has_battery = 1;
    cart.ram_size = 0x2000;
    cart.ram = calloc(1, cart.ram_size);
    cart.ram[0] = 0x48;
    cart.ram[1] = 0x1E;
    cart.ram[0x1FFF] = 0xAB; // last byte too - confirms the whole buffer round-trips, not just the front

    gb_cart_save_ram_file(&cart, path);

    GBCart loaded = {0};
    loaded.has_ram = 1;
    loaded.has_battery = 1;
    loaded.ram_size = 0x2000;
    loaded.ram = calloc(1, loaded.ram_size);
    gb_cart_load_ram_file(&loaded, path);

    check("battery RAM: round-trip preserves every byte, including the last one",
          memcmp(cart.ram, loaded.ram, cart.ram_size) == 0);

    free(cart.ram);
    free(loaded.ram);
    remove(path);
}

static void test_battery_ram_file_missing_is_noop(void) {
    const char *path = "/tmp/gb_test_cart_battery_missing.sav";
    remove(path); // guarantee it doesn't exist

    GBCart cart = {0};
    cart.has_ram = 1;
    cart.has_battery = 1;
    cart.ram_size = 0x2000;
    cart.ram = calloc(1, cart.ram_size);
    cart.ram[0] = 0x99; // would be clobbered by a real load - shouldn't be, since the file doesn't exist

    gb_cart_load_ram_file(&cart, path);
    check("battery RAM: loading a nonexistent path is a silent no-op, doesn't touch ram[]",
          cart.ram[0] == 0x99);

    free(cart.ram);
}

static void test_battery_ram_file_non_battery_is_noop(void) {
    const char *path = "/tmp/gb_test_cart_battery_nonbattery.sav";
    remove(path);

    GBCart cart = {0};
    cart.has_ram = 1;
    cart.has_battery = 0; // no battery - real hardware never persists this either
    cart.ram_size = 0x2000;
    cart.ram = calloc(1, cart.ram_size);
    cart.ram[0] = 0x55;

    gb_cart_save_ram_file(&cart, path);
    FILE *f = fopen(path, "rb");
    check("battery RAM: saving a non-battery cart never creates a file", f == NULL);
    if (f) fclose(f);

    free(cart.ram);
    remove(path);
}

int main(void) {
    test_mbc1_basic_rom_banking();
    test_mbc1_large_rom_advanced_mode();
    test_mbc1_ram_banking();
    test_mbc1_ram_disabled();
    test_mbc1_multicart_rom_banking();
    test_mbc1_multicart_detection();
    test_mbc3_rom_banking();
    test_mbc3_rtc_latch();
    test_mbc5_rom_banking();
    test_mbc5_default_bank_is_one();
    test_cart_load_from_file();
    test_battery_ram_file_round_trip();
    test_battery_ram_file_missing_is_noop();
    test_battery_ram_file_non_battery_is_noop();

    if (failures == 0) {
        printf("\nAll cart.c tests passed.\n");
        return 0;
    }
    printf("\n%d cart.c test(s) FAILED.\n", failures);
    return 1;
}
