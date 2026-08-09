#include "mmu.h"
#include "cart.h"
#include "ppu.h"
#include "timer.h"
#include "joypad.h"
#include "apu.h"
#include <stddef.h>

void (*gb_serial_output_hook)(uint8_t byte) = NULL;

// Echo RAM (0xE000-0xFDFF) is a real hardware mirror of WRAM's first
// 0x1E00 bytes (0xC000-0xDDFF), not a separate region - both addresses
// must observe the same underlying byte. Modeled here by redirecting
// reads/writes in the echo range onto the real WRAM storage rather than
// giving echo RAM its own backing bytes, so writes through either
// address are visible through both without needing to keep two copies
// in sync. Never touches the cartridge-routed ranges below (0x0000-
// 0x7FFF, 0xA000-0xBFFF are both under 0xE000), so no interaction there.
static uint16_t redirect_echo(uint16_t addr) {
    if (addr >= 0xE000 && addr <= 0xFDFF) return (uint16_t)(addr - 0x2000);
    return addr;
}

// CGB WRAM banking (SVBK, 0xFF70) - see cpu.h's wram_bank/svbk comment.
// Writing 0 selects bank 1 (not bank 0), a documented real-hardware
// quirk (pandocs' Memory_Map.md's SVBK entry) - so the *effective* bank
// is always 1-7, resolved here rather than stored pre-resolved, exactly
// like cart.c's own MBC1/MBC3 "register reads as 0 -> use bank 1"
// ROM-banking quirk resolves at read time, not write time.
static int wram_effective_bank(const GBCpu *cpu) {
    int bank = cpu->svbk & 0x07;
    return bank == 0 ? 1 : bank;
}

// Returns a pointer to the banked byte at a CGB WRAM address (0xD000-
// 0xDFFF, post echo-redirect) when a bank other than 1 is selected, or
// NULL when the access should fall through to cpu->memory unchanged
// (DMG mode, or bank 1 - which has always lived in `memory` itself, see
// cpu.h). Shared by gb_read_byte/gb_write_byte so both sides resolve
// the exact same byte.
static uint8_t *wram_banked_ptr(GBCpu *cpu, uint16_t addr) {
    if (!cpu->is_cgb || addr < 0xD000 || addr > 0xDFFF) return NULL;
    int bank = wram_effective_bank(cpu);
    if (bank == 1) return NULL;
    return &cpu->wram_bank[bank - 2][addr - 0xD000];
}

uint8_t gb_read_byte(GBCpu *cpu, uint16_t addr) {
    if (addr < 0x8000) return gb_cart_read(cpu->cart, addr);
    if (addr >= 0xA000 && addr < 0xC000) return gb_cart_read_ram(cpu->cart, addr);
    if (addr == 0xFF00) return gb_joypad_read(cpu->joypad);
    if (addr >= 0xFF04 && addr <= 0xFF07) return gb_timer_read(cpu->timer, addr);
    // The full 0xFF10-0xFF3F span, not just the individually-named
    // register addresses within it - 0xFF15, 0xFF1F, and 0xFF27-0xFF2F
    // are real, unconnected gaps in the register map that still need to
    // read back as $FF/ignore writes (apu.c's own default cases handle
    // that). Originally routed as two narrower ranges split around
    // NR52/Wave RAM, which silently let 0xFF27-0xFF2F fall through to
    // plain flat memory instead - found via Blargg's dmg_sound
    // 01-registers.gb, whose register r/w test walks this entire span.
    if (addr >= 0xFF10 && addr <= 0xFF3F) return gb_apu_read(cpu->apu, addr);
    // 0xFF4F (VBK) and 0xFF68-0xFF6B (BCPS/BCPD/OCPS/OCPD) are CGB-only
    // additions to the same PPU register block, gated on cpu->is_cgb
    // inside gb_ppu_read_reg/gb_ppu_write_reg themselves - see ppu.h's
    // own comment on why they're dispatched here rather than carved out
    // as their own inert-stub exception the way SVBK (mmu.c-owned) is.
    if ((addr >= 0xFF40 && addr <= 0xFF4B) || addr == 0xFF4F || (addr >= 0xFF68 && addr <= 0xFF6B)) {
        return gb_ppu_read_reg(cpu->ppu, cpu, addr);
    }
    // SC's bits 1-6 (pandocs' Serial_Data_Transfer.md: bit 7 transfer
    // enable, bit 1 CGB-only clock speed, bit 0 clock select - bits
    // 2-6 have no function at all) and IF's bits 5-7 (Interrupts.md:
    // only bits 0-4 correspond to a real interrupt source) are unused
    // and always read back as 1 regardless of what was written -
    // confirmed against Mooneye's own real-hardware-verified
    // unused_hwio-GS.gb (test_roms/mooneye/), whose exact masks
    // (0x7E/0xE0) match.
    if (addr == 0xFF02) return (uint8_t)(cpu->memory[addr] | 0x7E);
    if (addr == 0xFF0F) return (uint8_t)(cpu->memory[addr] | 0xE0);
    // SVBK (WRAM bank select) only exists in CGB mode - reads $FF in
    // DMG mode like every other register in this block (pandocs'
    // Power_Up_Sequence.md's hardware-registers table footnote: "only
    // available in CGB Mode, will read $FF in Non-CGB Mode"). The
    // unused upper bits (3-7) always read as 1, matching real hardware.
    if (addr == 0xFF70) return cpu->is_cgb ? (uint8_t)(cpu->svbk | 0xF8) : 0xFF;
    // Genuinely unmapped $FFxx I/O - no backing register exists at
    // all, so these always read back as 1 in every bit (same
    // unused_hwio-GS.gb ROM; $FF15/$FF1F/$FF27-$FF29 are the
    // equivalent APU-range gaps, already handled by apu.c's own
    // default case per the dmg_sound 01-registers.gb fix above).
    if (addr == 0xFF03 || (addr >= 0xFF08 && addr <= 0xFF0E) || (addr >= 0xFF4C && addr <= 0xFF7F)) {
        return 0xFF;
    }

    addr = redirect_echo(addr);
    {
        uint8_t *banked = wram_banked_ptr(cpu, addr);
        if (banked) return *banked;
    }
    if (addr >= 0xFEA0 && addr <= 0xFEFF) {
        // "Not usable" - real hardware's behavior here depends on PPU
        // state and hardware revision (see pandocs); this fixed 0xFF is
        // a placeholder, not a grounded model of the real quirk.
        return 0xFF;
    }
    // OAM DMA bus conflict (pandocs' OAM_DMA_Transfer.md: "the CPU can
    // access only HRAM" while a transfer is active) - reads of OAM
    // itself return a flat 0xFF, not whatever DMA happens to be
    // transferring that M-cycle. Verified against Gekkio's own
    // mooneye-gb reference emulator (hardware.rs's read(): `if
    // hw.oam_dma.is_active() { 0xff } else { hw.ppu.read_oam(addr) }`),
    // and against Mooneye's own real-hardware-verified jp_timing.gb
    // (test_roms/mooneye/), which deliberately positions the JP nn
    // target address's high byte at OAM's very first byte to probe
    // exactly this. Checked after cpu.c's gb_dma_tick() has already run
    // for this M-cycle, so this sees DMA's *current* (post-tick) state,
    // matching the reference model's own per-M-cycle ordering.
    if (addr >= 0xFE00 && addr <= 0xFE9F && cpu->dma_active) {
        return 0xFF;
    }
    // A second, independent OAM bus conflict: the PPU itself uses the
    // OAM bus during Modes 2/3, blocking the CPU's own access the same
    // way an active DMA transfer does (see gb_ppu_oam_blocked()'s own
    // comment, ppu.h/ppu.c, for the pandocs citation). Found via
    // Mooneye's own acceptance/ppu/intr_2_oam_ok_timing.gb
    // (test_roms/mooneye/), which measures exactly how long OAM stays
    // unreadable after a Mode 2 STAT interrupt.
    if (addr >= 0xFE00 && addr <= 0xFE9F && cpu->ppu && gb_ppu_oam_blocked(cpu->ppu, 0)) {
        return 0xFF;
    }
    // VRAM ($8000-$9FFF) bus conflict: the PPU itself uses that bus
    // during Mode 3 (Drawing) only - unlike OAM, Mode 2 (OAM scan)
    // doesn't touch VRAM at all, so it stays readable then. See
    // gb_ppu_vram_blocked()'s own comment (ppu.h/ppu.c) for the
    // Mooneye ROM (lcdon_timing-GS.gb) that found this had never been
    // implemented at all.
    if (addr >= 0x8000 && addr <= 0x9FFF && cpu->ppu && gb_ppu_vram_blocked(cpu->ppu, 0)) {
        return 0xFF;
    }
    // CGB VRAM banking (VBK, 0xFF4F): bank 0 stays in cpu->memory as
    // always; only a selected bank 1 redirects here. The blocking check
    // just above is bank-independent (real hardware contests the bus
    // during Mode 3 regardless of which bank is selected), so this comes
    // after it, not instead of it.
    if (addr >= 0x8000 && addr <= 0x9FFF && cpu->is_cgb && cpu->ppu && (cpu->ppu->vbk & 1)) {
        return cpu->ppu->vram_bank1[addr - 0x8000];
    }
    return cpu->memory[addr];
}

void gb_write_byte(GBCpu *cpu, uint16_t addr, uint8_t val) {
    if (addr < 0x8000) { gb_cart_write_ctrl(cpu->cart, addr, val); return; }
    if (addr >= 0xA000 && addr < 0xC000) { gb_cart_write_ram(cpu->cart, addr, val); return; }
    if (addr == 0xFF00) { gb_joypad_write(cpu->joypad, val); return; }
    if (addr >= 0xFF04 && addr <= 0xFF07) { gb_timer_write(cpu->timer, cpu, addr, val); return; }
    if (addr >= 0xFF10 && addr <= 0xFF3F) { gb_apu_write(cpu->apu, addr, val); return; }
    if ((addr >= 0xFF40 && addr <= 0xFF4B) || addr == 0xFF4F || (addr >= 0xFF68 && addr <= 0xFF6B)) {
        gb_ppu_write_reg(cpu->ppu, cpu, addr, val);
        return;
    }
    // SVBK - see the matching read-side comment. Ignored entirely in
    // DMG mode, exactly like the surrounding inert register block.
    if (addr == 0xFF70) { if (cpu->is_cgb) cpu->svbk = val & 0x07; return; }
    // Genuinely unmapped - see the matching read-side comment above;
    // no backing register, so the write has nowhere to go.
    if (addr == 0xFF03 || (addr >= 0xFF08 && addr <= 0xFF0E) || (addr >= 0xFF4C && addr <= 0xFF7F)) {
        return;
    }

    addr = redirect_echo(addr);
    {
        uint8_t *banked = wram_banked_ptr(cpu, addr);
        if (banked) { *banked = val; return; }
    }
    if (addr >= 0xFEA0 && addr <= 0xFEFF) {
        return; // "not usable" - see the read-side comment above
    }
    // Same OAM DMA bus conflict as gb_read_byte's read side, but for
    // writes: the CPU's own write is simply dropped (matching the
    // reference model's `if !hw.oam_dma.is_active() { write_oam(...) }`
    // - no `else` branch, meaning an active-DMA write has no effect at
    // all). Confirmed against Mooneye's push_timing.gb: it deliberately
    // points SP into OAM and preloads DMA's own source bytes with a
    // known marker value, so a dropped write leaves OAM holding
    // whatever DMA's own concurrent, unaffected copy already wrote
    // there - not the CPU's value, and not left "unchanged" either,
    // since DMA keeps copying regardless of what the CPU attempts.
    if (addr >= 0xFE00 && addr <= 0xFE9F && cpu->dma_active) {
        return;
    }
    // The PPU-mode side of the same conflict - see gb_read_byte()'s
    // matching comment.
    if (addr >= 0xFE00 && addr <= 0xFE9F && cpu->ppu && gb_ppu_oam_blocked(cpu->ppu, 1)) {
        return;
    }
    // VRAM bus conflict, write side - see gb_read_byte()'s matching
    // comment.
    if (addr >= 0x8000 && addr <= 0x9FFF && cpu->ppu && gb_ppu_vram_blocked(cpu->ppu, 1)) {
        return;
    }
    // CGB VRAM banking, write side - see gb_read_byte()'s matching comment.
    if (addr >= 0x8000 && addr <= 0x9FFF && cpu->is_cgb && cpu->ppu && (cpu->ppu->vbk & 1)) {
        cpu->ppu->vram_bank1[addr - 0x8000] = val;
        return;
    }
    if (addr == 0xFF02 && (val & 0x81) == 0x81) {
        // Serial transfer start, internal clock: Blargg's test ROMs use
        // exactly this to emit one output character via SB (0xFF01)
        // without a real link-cable peer attached - see mmu.h.
        if (gb_serial_output_hook) gb_serial_output_hook(cpu->memory[0xFF01]);
    }
    cpu->memory[addr] = val;
}

// Advances the requested->starting->active pipeline by exactly one
// M-cycle, in the same order as Gekkio's mooneye-gb reference emulator
// (hardware.rs's emulate_oam_dma(), called once per M-cycle from every
// generic_mem_cycle): (1) if a transfer is already active, copy this
// M-cycle's one byte using the *current* addr and advance it, stopping
// once 160 bytes are done; (2) only then, if a transfer was scheduled
// to start, actually start it (addr set, but no byte copied this same
// M-cycle - that happens next call); (3) only then, if $FF46 was
// written last M-cycle, advance that request into "starting". Doing
// these in this order (not, say, starting-then-copying) is what
// reproduces the real 2 M-cycle gap between the $FF46 write and the
// first byte actually copying - cross-checked against Mooneye's own
// push_timing.s padding arithmetic (test_roms/mooneye/README.md), not
// just copied from the reference source blind.
//
// The copy itself bypasses both the CPU-facing conflict checks above:
// the source read goes through the ordinary gb_read_byte() (safe,
// since a DMA source page is always <= 0xDF, so addr never lands in
// $FE00-$FE9F and can never recurse into this same conflict logic),
// but the OAM write goes *directly* to cpu->memory rather than through
// gb_write_byte() - going through it would hit the very "drop the
// write while DMA is active" rule above and make DMA unable to write
// its own bytes.
void gb_dma_tick(GBCpu *cpu) {
    if (cpu->dma_active) {
        // DMA's own address generator has no special case for OAM/I-O -
        // unlike the CPU's normal bus decoder, it just keeps counting
        // through the same 13-bit WRAM address space echo RAM mirrors
        // with. A source page of $E0-$FF (nominally OAM/unusable/I-O -
        // pandocs' OAM_DMA_Transfer.md only documents $00-$DF as valid)
        // actually reads WRAM at $C000-$DFFF instead, real hardware's
        // page with bit 5 (0x20) cleared - confirmed against Gekkio's
        // own mooneye-gb (hardware.rs's emulate_oam_dma(): source pages
        // 0xe0..=0xef and 0xf0..=0xff route to the exact same
        // work_ram.read_lower()/read_upper() calls as 0xc0..=0xcf/
        // 0xd0..=0xdf), and against this project's own
        // test_roms/mooneye/acceptance/oam_dma/sources-GS.gb, which
        // sources DMA from page $FE/$FF and asserts OAM ends up with
        // whatever was written to $DE00/$DF00 beforehand.
        uint8_t page = cpu->dma_source_page;
        if (page >= 0xC0) page &= 0xDF;
        uint16_t src = (uint16_t)((page << 8) + cpu->dma_progress);
        cpu->memory[0xFE00 + cpu->dma_progress] = gb_read_byte(cpu, src);
        cpu->dma_progress++;
        if (cpu->dma_progress >= 160) {
            cpu->dma_active = 0;
        }
    }
    if (cpu->dma_starting_pending) {
        cpu->dma_active = 1;
        cpu->dma_source_page = cpu->dma_starting_value;
        cpu->dma_progress = 0;
        cpu->dma_starting_pending = 0;
    }
    if (cpu->dma_request_pending) {
        cpu->dma_starting_pending = 1;
        cpu->dma_starting_value = cpu->dma_request_value;
        cpu->dma_request_pending = 0;
    }
}

// Advances every subsystem whose own state depends on precise sub-
// instruction timing - DMA, the timer, the PPU, and the APU - by
// exactly one real M-cycle (4 T-states), called once per real M-cycle
// of CPU execution (cpu.c's every opcode handler, not once per whole
// instruction the way main.c/sdl's driver loop used to call
// gb_ppu_step()/gb_timer_step()/gb_apu_step() directly). This is the
// same per-M-cycle interleaving Gekkio's own mooneye-gb reference
// emulator uses throughout (every register access goes through its
// shared generic_mem_cycle/timer_mem_cycle, not a curated subset of
// "timing-sensitive" opcodes) - the exact lesson this project's own
// "Timer M-cycle precision: attempted, reverted" investigation
// (docs/GAMEBOY_ROADMAP.md) found the hard way: a timer that only
// self-ticks a dozen or so opcode handlers (matching what sufficed for
// DMA) leaves its own counter wrong at every *other* opcode's TAC/TIMA
// write, since edge-detection needs it exactly current at that instant,
// not just correct in total by the end of an instruction. Folding the
// PPU in too closes the matching per-dot gap `acceptance/ppu/`'s
// remaining timing ROMs (test_roms/mooneye/) found - mooneye-gb's own
// ppu.rs requests Mode 0's STAT interrupt one T-state before the real
// Mode 3->0 switch, something only reachable with per-T-state PPU
// stepping, not the once-per-instruction lump sum ppu.h's own comment
// already documented as a scope limit.
//
// timer/ppu/apu are each NULL-guarded: tests/test_cpu.c's own minimal
// GBCpu (built to exercise cpu.c/mmu.c in isolation, no PPU/timer/APU
// wired up at all) still needs gb_cpu_step() to work without crashing -
// gb_dma_tick() itself has never needed this guard since DMA state
// lives directly on GBCpu, not behind one of these optional pointers.
void gb_mcycle_tick(GBCpu *cpu) {
    gb_dma_tick(cpu);
    if (cpu->timer) gb_timer_step(cpu->timer, cpu, 4);
    if (cpu->ppu) gb_ppu_step(cpu->ppu, cpu, 4);
    if (cpu->apu) gb_apu_step(cpu->apu, cpu, 4);
}
