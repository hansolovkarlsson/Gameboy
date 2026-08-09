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

// FF55 write handler (HDMA1-4 latching happens directly at their own
// gb_write_byte() call sites, right below). pandocs' CGB_Registers.md
// "FF55 — HDMA5": writing zero to bit 7 while an HBlank transfer is
// already active cancels it (doesn't start a fresh GDMA); HDMA1-4 are
// explicitly *not* reset by this - matches leaving hdma_src/hdma_dst/
// hdma_remaining untouched here. Otherwise this arms a new transfer,
// snapshotting the staging latches into the real transfer pointers -
// changes to FF51-54 after this point have no effect on the transfer
// now in progress, only on whatever the *next* FF55 write arms.
static void start_or_cancel_hdma(GBCpu *cpu, uint8_t val) {
    if (cpu->hdma_active && cpu->hdma_mode == 1 && !(val & 0x80)) {
        cpu->hdma_active = 0;
        return;
    }
    cpu->hdma_src = (uint16_t)(((cpu->hdma_src_hi << 8) | cpu->hdma_src_lo) & 0xFFF0);
    cpu->hdma_dst = (uint16_t)(0x8000 | (((cpu->hdma_dst_hi << 8) | cpu->hdma_dst_lo) & 0x1FF0));
    cpu->hdma_remaining = (uint16_t)(((val & 0x7F) + 1) * 16);
    cpu->hdma_mode = (uint8_t)(val >> 7);
    cpu->hdma_active = 1;
    // GDMA has no HBlank gating at all - it's one uninterruptible block
    // covering the whole transfer, armed immediately here. HBlank mode
    // leaves this at 0; gb_hdma_hblank_trigger() (ppu.c's Mode 3->0
    // transition) arms the first real block.
    cpu->hdma_block_bytes_left = (cpu->hdma_mode == 0) ? cpu->hdma_remaining : 0;
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
    // KEY1/SPD (double-speed mode) only exists in CGB mode, same footnote
    // as SVBK above. Bit 7 (current speed) is read-only, bit 0 (switch
    // armed) is read/write; bits 1-6 are unused and always read as 1 -
    // pandocs' CGB_Registers.md "FF4D — KEY1/SPD", confirmed against
    // gbdev/hardware.inc's B_SPD_DOUBLE=7/B_SPD_PREPARE=0 bit constants.
    if (addr == 0xFF4D) return cpu->is_cgb ? (uint8_t)((cpu->key1 & 0x81) | 0x7E) : 0xFF;
    // HDMA1-4 (0xFF51-0xFF54): pure write-only staging latches on real
    // hardware, no readback path at all - pandocs' CGB_Registers.md
    // tags each one "[write-only]" explicitly. Always 0xFF, CGB or not.
    if (addr >= 0xFF51 && addr <= 0xFF54) return 0xFF;
    // HDMA5 (0xFF55): bit 7 = "not active" (1) / "active" (0); bits 0-6
    // = remaining $10-byte blocks minus 1 - pandocs' CGB_Registers.md
    // "FF55 — HDMA5". Three distinct states, not two: genuinely active
    // (bit 7 clear, real remaining count); manually cancelled mid-HBlank-
    // transfer (bit 7 forced to 1, but the lower 7 bits *still* report
    // how many blocks were left at cancellation - pandocs' own explicit
    // wording, not the same as "completed"); and never-started/naturally-
    // completed (flat $FF). hdma_remaining is always a multiple of 16 by
    // construction (start_or_cancel_hdma() below, and gb_hdma_tick() only
    // clears hdma_active exactly when it reaches 0), so >>4 is an exact
    // block count and "still nonzero but not active" unambiguously means
    // "was cancelled," never "just completed."
    if (addr == 0xFF55) {
        if (!cpu->is_cgb) return 0xFF;
        if (cpu->hdma_active) return (uint8_t)(((cpu->hdma_remaining >> 4) - 1) & 0x7F);
        if (cpu->hdma_remaining) return (uint8_t)((((cpu->hdma_remaining >> 4) - 1) & 0x7F) | 0x80);
        return 0xFF;
    }
    // RP (infrared port) - register-level fidelity only (no real IR
    // peer exists to communicate with - docs/HARDWARE_REFERENCE.md's RP
    // section). Bits 7-6/0 reflect whatever was last written; bit 1
    // (receiving) is forced to 1 unconditionally ("no signal detected" -
    // the honest consequence of there being no peer, not a real read of
    // anything); bits 5-2 are unused and read as 1, this project's usual
    // convention for unused bits (SVBK/KEY1/IF/SC).
    if (addr == 0xFF56) return cpu->is_cgb ? (uint8_t)((cpu->rp & 0xC1) | 0x3E) : 0xFF;
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
    // KEY1/SPD - see the matching read-side comment. Only bit 0 (armed) is
    // writable; bit 7 (current speed) is set only by gb_op_stop()'s actual
    // switch (cpu.c), never by a direct write.
    if (addr == 0xFF4D) { if (cpu->is_cgb) cpu->key1 = (uint8_t)((cpu->key1 & 0x80) | (val & 0x01)); return; }
    // HDMA1-4 - pure staging latches, see the matching read-side
    // comment. Ignored entirely in DMG mode.
    if (addr == 0xFF51) { if (cpu->is_cgb) cpu->hdma_src_hi = val; return; }
    if (addr == 0xFF52) { if (cpu->is_cgb) cpu->hdma_src_lo = val; return; }
    if (addr == 0xFF53) { if (cpu->is_cgb) cpu->hdma_dst_hi = val; return; }
    if (addr == 0xFF54) { if (cpu->is_cgb) cpu->hdma_dst_lo = val; return; }
    // HDMA5 - see start_or_cancel_hdma()'s own comment above.
    if (addr == 0xFF55) { if (cpu->is_cgb) start_or_cancel_hdma(cpu, val); return; }
    // RP - see the matching read-side comment. Only bits 7-6/0 are
    // stored; bit 1 (receiving) is read-only, never affected by a write
    // even if the game writes a 1 there.
    if (addr == 0xFF56) { if (cpu->is_cgb) cpu->rp = (uint8_t)(val & 0xC1); return; }
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

// Advances an active HDMA/GDMA transfer (cpu.h's hdma_* comment) by one
// M-cycle's worth of bytes. Rate is a flat 2 bytes/M-cycle at normal
// speed, 1 byte/M-cycle at double speed - pandocs' CGB_Registers.md
// "Transfer Timings": a $10-byte block takes 8 M-cycles at Normal Speed
// but 16 "fast" M-cycles at Double Speed, i.e. it stays at the same
// *real-time* rate regardless of CPU speed, exactly the same "needs
// double the M-cycles when the CPU runs 2x faster" pattern already
// applied to the PPU/APU throttle in gb_mcycle_tick() below.
void gb_hdma_tick(GBCpu *cpu) {
    if (!cpu->hdma_block_bytes_left) return;
    int rate = (cpu->is_cgb && (cpu->key1 & 0x80)) ? 1 : 2;
    for (int i = 0; i < rate && cpu->hdma_block_bytes_left; i++) {
        uint8_t byte = gb_read_byte(cpu, cpu->hdma_src);
        // Destination write bypasses gb_write_byte()/Mode-3 VRAM
        // blocking entirely - pandocs: GDMA "blindly attempts to copy
        // the data, even if the LCD controller is currently accessing
        // VRAM." Same "DMA writes go direct" reasoning gb_dma_tick()
        // above already uses for OAM. VBK is read live here (not
        // snapshotted when the transfer was armed) per pandocs' own
        // warning against changing it mid-transfer - implying it *does*
        // take effect immediately if a game ignores that warning.
        uint8_t *dst = (cpu->ppu && (cpu->ppu->vbk & 1))
            ? &cpu->ppu->vram_bank1[cpu->hdma_dst - 0x8000]
            : &cpu->memory[cpu->hdma_dst];
        *dst = byte;
        cpu->hdma_src++;
        cpu->hdma_dst++;
        cpu->hdma_remaining--;
        cpu->hdma_block_bytes_left--;
    }
    if (!cpu->hdma_remaining) cpu->hdma_active = 0;
}

// Arms one 16-byte HBlank-DMA block - called from ppu.c's own Mode 3->0
// transition, the one real per-scanline HBlank entry point. Pandocs'
// CGB_Registers.md: "the HBlank DMA transfers $10 bytes of data during
// each HBlank" (LY=0-143 only - ppu.c only reaches Mode 0 through this
// transition on visible lines, never during VBlank, so no separate
// LY check is needed here). !hdma_block_bytes_left guards against
// re-arming mid-block (shouldn't be possible - a block always fully
// drains within the M-cycles of a single HBlank - but keeps this
// idempotent regardless). !cpu->halted is this implementation's chosen
// reading of pandocs' "upon halting the CPU, the transfer will also be
// halted" - documented as a simplification (not a precise mid-freeze
// resume) in docs/HARDWARE_REFERENCE.md's HDMA section.
void gb_hdma_hblank_trigger(GBCpu *cpu) {
    if (cpu->is_cgb && cpu->hdma_active && cpu->hdma_mode == 1
        && !cpu->hdma_block_bytes_left && !cpu->halted) {
        cpu->hdma_block_bytes_left = cpu->hdma_remaining < 16 ? cpu->hdma_remaining : 16;
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
// CGB double-speed mode (KEY1, cpu.h's key1/speed_switch_pause comment):
// the CPU, timer/DIV, and OAM DMA (gb_dma_tick() above) all genuinely
// speed up 2x in double-speed mode, and every one of them is already
// driven exactly 1:1 with real CPU M-cycles here with no throttling -
// so they get the 2x speedup for free, no code change needed. The LCD
// (PPU) and sound (APU) are documented to keep running at the *same*
// real-time rate regardless of CPU speed (pandocs' CGB_Registers.md
// KEY1 section), so when double speed is active they're each handed
// half as many T-states per M-cycle - they're being called twice as
// often in real time, so halving keeps their real-time rate constant.
void gb_mcycle_tick(GBCpu *cpu) {
    gb_dma_tick(cpu);
    gb_hdma_tick(cpu);
    if (cpu->timer) gb_timer_step(cpu->timer, cpu, 4);
    int video_audio_cycles = (cpu->is_cgb && (cpu->key1 & 0x80)) ? 2 : 4;
    if (cpu->ppu) gb_ppu_step(cpu->ppu, cpu, video_audio_cycles);
    if (cpu->apu) gb_apu_step(cpu->apu, cpu, video_audio_cycles);
}
