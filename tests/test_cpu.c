#include <stdio.h>
#include <string.h>
#include "../src/cpu.h"
#include "../src/cart.h"
#include "../src/ppu.h"
#include "../src/timer.h"

// Direct unit test for the "HALT immediately after EI" sub-case of the
// HALT bug - grounded against pandocs' halt.md: real hardware doesn't
// apply the generic double-fetch halt bug here. Instead HALT is
// effectively canceled outright (pc rewound back to HALT's own
// address), the already-pending interrupt dispatches completely
// normally on the *next* step (once EI's one-instruction delay has
// resolved to ime=1), using that unadvanced pc as its return address -
// so RETI naturally resumes execution back at the same HALT, which by
// then sees ime=1 for real and halts properly, per pandocs' own
// description: "the interrupt is serviced and the handler called, but
// the interrupt returns to the halt, which is executed again."
//
// Found via a real ROM (test_roms/tobutobugirl/) whose main
// loop's own "ei; halt" idiom hit exactly this - treating it as the
// generic halt_bug case instead corrupted the stack by 2 bytes (see
// that ROM's own README.md for the full story) and eventually crashed
// on an illegal opcode after jumping to garbage.
//
// gb_cpu_step() needs gb_read_byte()/gb_write_byte() (mmu.c) but no
// cart/ppu/timer/joypad/apu at all, as long as the test keeps every
// address it touches inside flat memory (0x8000+, avoiding the
// specially-routed I/O ranges) - same minimal-dependency reasoning
// test_apu.c already uses.

static int failures = 0;

static void check(const char *name, int condition) {
    if (condition) {
        printf("OK   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        failures++;
    }
}

static void test_halt_after_ei(void) {
    uint8_t memory[65536] = {0};
    // The real Timer vector (0x0050) is a ROM address (<0x8000), routed
    // through gb_cart_read() rather than the flat memory[] array below -
    // a minimal no-MBC cart backs it, same GBCart-struct-literal
    // approach test_cart.c already uses to bypass gb_cart_load()'s file
    // I/O entirely.
    uint8_t rom[0x8000] = {0};
    rom[0x0050] = 0xD9; // RETI - a minimal Timer-interrupt handler stub

    GBCart cart = {0};
    cart.rom = rom;
    cart.rom_size = sizeof(rom);
    cart.mbc_type = GB_MBC_NONE;

    GBCpu cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.memory = memory;
    cpu.cart = &cart;
    gb_cpu_init_tables();
    gb_cpu_reset(&cpu);

    // A tiny "ei; halt" idle loop in WRAM (0xC000) - enough to exercise
    // the whole dispatch/return cycle without needing any real
    // registered-handler machinery.
    memory[0xC000] = 0xFB; // EI
    memory[0xC001] = 0x76; // HALT

    cpu.pc = 0xC000;
    cpu.sp = 0xD000;
    cpu.ime = 0;
    memory[0xFFFF] = 0x04; // IE: Timer (bit 2) enabled
    memory[0xFF0F] = 0x04; // IF: Timer already pending

    gb_cpu_step(&cpu); // EI
    check("EI: ime still 0 immediately after (one-instruction delay)", cpu.ime == 0);
    check("EI: pc advanced past EI to HALT", cpu.pc == 0xC001);

    gb_cpu_step(&cpu); // HALT, the ei-delay sub-case
    check("HALT-after-EI: does not set the generic halt_bug", cpu.halt_bug == 0);
    check("HALT-after-EI: does not actually halt", cpu.halted == 0);
    check("HALT-after-EI: pc rewound back to HALT's own address", cpu.pc == 0xC001);

    uint16_t sp_before_dispatch = cpu.sp;
    gb_cpu_step(&cpu); // ime is now 1 (EI's delay resolved) - real dispatch
    check("dispatch: pc jumped to the Timer vector", cpu.pc == 0x0050);
    check("dispatch: ime cleared for the handler", cpu.ime == 0);
    check("dispatch: pushed the HALT instruction's own address (0xC001), not past it",
          (uint16_t)(memory[cpu.sp] | (memory[(uint16_t)(cpu.sp + 1)] << 8)) == 0xC001);
    check("dispatch: sp decreased by exactly 2 (a balanced single push)",
          cpu.sp == (uint16_t)(sp_before_dispatch - 2));

    gb_cpu_step(&cpu); // RETI
    check("RETI: returns to the HALT instruction, not somewhere else", cpu.pc == 0xC001);
    check("RETI: sp rebalanced back to its pre-dispatch value", cpu.sp == sp_before_dispatch);
    check("RETI: ime set", cpu.ime == 1);

    gb_cpu_step(&cpu); // HALT retried, this time with a genuine ime=1
    check("HALT retried: halts for real now that ime is genuinely 1", cpu.halted == 1);
    check("HALT retried: still no halt_bug", cpu.halt_bug == 0);
}

// Direct unit test for gb_dma_tick()'s (mmu.c) WRAM-mirror source quirk:
// unlike the CPU's normal bus decoder, OAM DMA's own address generator
// has no special case for OAM/I-O, so a source page of $E0-$FF actually
// reads WRAM at $C000-$DFFF instead (real hardware's page with bit 5
// cleared) - found via test_roms/mooneye/acceptance/oam_dma/
// sources-GS.gb, confirmed against Gekkio's own mooneye-gb
// (hardware.rs's emulate_oam_dma()). See mmu.c's own gb_dma_tick()
// comment for the full citation.
static void test_dma_wram_mirror_source(void) {
    uint8_t memory[65536] = {0};
    GBCpu cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.memory = memory;

    memory[0xDE00] = 0xAB; // what source page $FE should really read
    memory[0xDF00] = 0xCD; // what source page $FF should really read
    memory[0xFE00] = 0x00; // real OAM at the same offset - must NOT be what gets copied

    cpu.dma_active = 1;
    cpu.dma_source_page = 0xFE;
    cpu.dma_progress = 0;
    gb_dma_tick(&cpu);
    check("DMA: source page $FE reads WRAM at $DE00, not literal $FE00",
          memory[0xFE00] == 0xAB);

    cpu.dma_active = 1;
    cpu.dma_source_page = 0xFF;
    cpu.dma_progress = 0;
    gb_dma_tick(&cpu);
    check("DMA: source page $FF reads WRAM at $DF00", memory[0xFE00] == 0xCD);

    memory[0xC000] = 0x42; // a legitimate, already-valid source page - must be unaffected by the >=0xC0 masking
    cpu.dma_active = 1;
    cpu.dma_source_page = 0xC0;
    cpu.dma_progress = 0;
    gb_dma_tick(&cpu);
    check("DMA: source page $C0 (already valid) is untouched by the mirror-fixup",
          memory[0xFE00] == 0x42);
}

// Direct unit test for CGB double-speed mode (KEY1, 0xFF4D) - pandocs'
// CGB_Registers.md. Covers the two halves that don't need a real ROM:
// (1) gb_op_stop()'s armed-vs-unarmed branch and the resulting
// speed_switch_pause drain in gb_cpu_step(), and (2) gb_mcycle_tick()'s
// (mmu.c) resulting PPU/APU throttle, observed via GBPpu.dots (ppu.c's
// own per-T-state counter, incremented once per gb_ppu_step() call
// before any mode-transition logic - see ppu.c's gb_ppu_step()).
static void test_key1_double_speed(void) {
    uint8_t memory[65536] = {0};
    uint8_t rom[0x8000] = {0};
    GBCart cart = {0};
    cart.rom = rom;
    cart.rom_size = sizeof(rom);
    cart.mbc_type = GB_MBC_NONE;

    // gb_op_stop() unconditionally calls gb_timer_reset_div() (real
    // hardware always resets the system counter on STOP, armed or not),
    // so a real (zeroed is fine) GBTimer is needed even though this test
    // isn't exercising timer behavior itself.
    GBTimer timer;
    memset(&timer, 0, sizeof(timer));

    GBCpu cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.memory = memory;
    cpu.cart = &cart;
    cpu.timer = &timer;
    cpu.is_cgb = 1;
    gb_cpu_init_tables();
    gb_cpu_reset(&cpu);

    // --- KEY1 register read/write masking (mmu.c) ---
    gb_write_byte(&cpu, 0xFF4D, 0xFF); // only bit 0 (armed) is writable
    check("KEY1 internal: armed bit set, speed bit still 0", (cpu.key1 & 0x81) == 0x01);
    check("KEY1 read: armed(1) | unused bits(1-6, read as 1) | speed bit(0) = 0x7F",
          gb_read_byte(&cpu, 0xFF4D) == 0x7F);
    gb_write_byte(&cpu, 0xFF4D, 0x00);
    check("KEY1 write: armed bit clears", (cpu.key1 & 0x01) == 0);

    // --- STOP without armed bit: pre-existing low-power path, untouched ---
    cpu.pc = 0xC000;
    memory[0xC000] = 0x10; // STOP
    memory[0xC001] = 0x00; // padding byte
    cpu.key1 = 0x00;
    gb_cpu_step(&cpu);
    check("STOP unarmed: key1 untouched", cpu.key1 == 0x00);
    check("STOP unarmed: sets the pre-existing stopped flag", cpu.stopped == 1);
    check("STOP unarmed: does not start a speed-switch pause", cpu.speed_switch_pause == 0);

    // --- STOP with armed bit: real speed switch ---
    memset(&cpu, 0, sizeof(cpu));
    cpu.memory = memory;
    cpu.cart = &cart;
    cpu.timer = &timer;
    cpu.is_cgb = 1;
    gb_cpu_reset(&cpu);
    cpu.pc = 0xC000;
    cpu.key1 = 0x01; // armed, currently normal speed
    gb_cpu_step(&cpu); // STOP
    check("STOP armed: speed bit flips to double speed", (cpu.key1 & 0x80) == 0x80);
    check("STOP armed: armed bit auto-clears", (cpu.key1 & 0x01) == 0);
    check("STOP armed: starts the 2050 M-cycle freeze", cpu.speed_switch_pause == 2050);

    // The freeze drains one M-cycle per gb_cpu_step() call, dispatching
    // no opcode (pc must not move) until fully drained.
    uint16_t pc_during_pause = cpu.pc;
    int pc_moved_during_pause = 0;
    for (int i = 0; i < 2049; i++) {
        gb_cpu_step(&cpu);
        if (cpu.pc != pc_during_pause) pc_moved_during_pause = 1;
    }
    check("freeze: pc never moves while draining (2049 steps)", !pc_moved_during_pause);
    check("freeze: exactly one M-cycle left", cpu.speed_switch_pause == 1);
    gb_cpu_step(&cpu); // drains the last M-cycle
    check("freeze: fully drained after 2050 steps", cpu.speed_switch_pause == 0);
    gb_cpu_step(&cpu); // first real instruction dispatch after the freeze
    check("freeze: normal dispatch resumes once drained", cpu.pc != pc_during_pause);

    // --- gb_mcycle_tick()'s resulting PPU throttle (mmu.c) ---
    GBPpu ppu;
    memset(&ppu, 0, sizeof(ppu));
    ppu.lcdc = 0x80; // LCD must be on, or gb_ppu_step() early-returns entirely
    cpu.ppu = &ppu;

    cpu.is_cgb = 0;
    cpu.key1 = 0x80; // irrelevant in DMG mode
    ppu.dots = 0;
    gb_mcycle_tick(&cpu);
    check("mcycle_tick: DMG mode always advances PPU by 4 T-states", ppu.dots == 4);

    cpu.is_cgb = 1;
    cpu.key1 = 0x00; // CGB but normal speed
    ppu.dots = 0;
    gb_mcycle_tick(&cpu);
    check("mcycle_tick: CGB normal speed advances PPU by 4 T-states", ppu.dots == 4);

    cpu.is_cgb = 1;
    cpu.key1 = 0x80; // CGB double speed
    ppu.dots = 0;
    gb_mcycle_tick(&cpu);
    check("mcycle_tick: CGB double speed advances PPU by only 2 T-states", ppu.dots == 2);
}

int main(void) {
    test_halt_after_ei();
    test_dma_wram_mirror_source();
    test_key1_double_speed();

    if (failures == 0) {
        printf("\nAll cpu.c tests passed.\n");
        return 0;
    }
    printf("\n%d test(s) FAILED.\n", failures);
    return 1;
}
