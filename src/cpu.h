#ifndef _GB_CPU_H
#define _GB_CPU_H

#include <stdint.h>

// See docs/GAMEBOY_ROADMAP.md's "Architecture decision" section for why
// this is a standalone core rather than sharing code/structs with
// cpm/emu/src/z80.h - the two CPUs are related but not identical, and this
// struct reflects the SM83's own real register file: no IX/IY, no
// alternate register set (both are Z80-only), no I/O ports (the SM83 has
// none - all device access is memory-mapped).
typedef struct GBCpu {
    union { struct { uint8_t f, a; }; uint16_t af; };
    union { struct { uint8_t c, b; }; uint16_t bc; };
    union { struct { uint8_t e, d; }; uint16_t de; };
    union { struct { uint8_t l, h; }; uint16_t hl; };

    uint16_t sp;
    uint16_t pc;

    // Effective runtime mode, resolved once at startup (main.c/sdl's
    // main.c, via gb_resolve_cgb_mode() in cart.c) from the cartridge's
    // header flag and any --mode override: 1 = CGB, 0 = DMG. Read by
    // gb_cpu_reset() (CGB post-boot register/register-default state),
    // mmu.c (WRAM/VRAM bank routing, CGB-only register carve-outs), and
    // ppu.c (CGB tile-attribute/palette rendering path). Never inferred
    // implicitly from the cart inside cpu.c/ppu.c/mmu.c themselves - see
    // cart.h's gb_resolve_cgb_mode() for the one place that decision is
    // made.
    uint8_t is_cgb;

    // Interrupt Master Enable - set/cleared by EI/DI/RETI, and by the
    // interrupt dispatch logic itself. EI's real hardware behavior
    // delays the actual enable by one instruction (see pandocs'
    // Interrupts page) - ime_pending/ei_delay implement that.
    uint8_t ime;
    uint8_t ime_pending; // EI was just executed; take effect after the *next* instruction

    // Set by gb_cpu_step() for the single instruction immediately
    // following an EI whose one-instruction delay hasn't resolved yet
    // (i.e. this instruction is executing with ime still 0, but ime_pending
    // is about to turn it on right after) - read only by gb_op_ld_r_r()'s
    // HALT (0x76) case, which needs to distinguish "HALT right after EI"
    // from the ordinary halt-bug case (pandocs' halt.md documents these
    // as genuinely different real-hardware behaviors). Deliberately a
    // separate field from ime_pending itself, not a reuse of it - EI's
    // own opcode handler writes ime_pending too, and conflating the two
    // would risk that write clobbering this one's meaning.
    uint8_t ei_delay_active;

    // Set by gb_op_di() when DI is the instruction immediately
    // following an EI whose one-instruction delay hasn't resolved yet -
    // lets gb_cpu_step() skip applying that stale delayed enable at the
    // end of this same step, so "ei; di" (rapid or otherwise) never
    // actually turns interrupts on even for the single instant between
    // the two - real hardware's documented behavior, and distinct from
    // ei_delay_active above (that one's read *during* the instruction
    // after EI; this one's read *after* it, by the same gb_cpu_step()
    // call that captured the pending enable at its own start). Verified
    // against Mooneye's real-hardware-verified acceptance/rapid_di_ei.gb
    // (test_roms/mooneye/).
    uint8_t di_cancels_ei_delay;

    // HALT: true once a HALT instruction has been executed. The run loop
    // is expected to stop advancing PC (just burn cycles) while this is
    // set, until an interrupt becomes pending. STOP is similar but also
    // real hardware requires a subsequent input/reset to leave it (not
    // modeled yet - no interrupt controller/joypad exists until Phase 4).
    uint8_t halted;
    uint8_t stopped;

    // The "HALT bug": a real hardware quirk where HALT executed with
    // IME=0 and a pending interrupt already latched (IE & IF != 0)
    // fails to advance PC afterward, causing the next opcode byte to be
    // fetched (and executed) twice. Implemented as of Phase 4 - see
    // cpu.c's gb_cpu_step() and pandocs' halt.md (fetched during
    // Phase 1, when this field was first added but left unused).
    uint8_t halt_bug;

    // OAM DMA state machine - real hardware transfers 160 bytes over 160
    // M-cycles (1/M-cycle), not instantly, and while a transfer is
    // active the CPU can only access HRAM ($FF80-$FFFE); any other
    // access it makes to OAM specifically is bus-conflicted (pandocs'
    // OAM_DMA_Transfer.md). Modeled as a 3-stage pipeline - `requested`
    // -> `starting` -> `active` - matching Gekkio's own mooneye-gb
    // reference emulator (core/src/hardware.rs's OamDma/emulate_oam_dma,
    // itself verified against real hardware and against this same
    // Mooneye test suite): writing $FF46 doesn't start the transfer
    // immediately, it takes 2 more M-cycles (via the requested->starting
    // ->active handoff, one stage advanced per M-cycle by gb_dma_tick())
    // before the first byte actually copies - see gb_dma_tick() (mmu.c)
    // for the exact per-M-cycle order this reproduces. Each pipeline
    // stage is a separate present/value pair rather than an Option-style
    // sentinel (e.g. -1 for "nothing pending") specifically so a
    // zero-initialized GBCpu is already a valid "no DMA in flight" state,
    // consistent with halted/stopped/ime/every other flag above - not
    // dependent on every construction site remembering to call
    // gb_cpu_reset() before the first gb_cpu_step().
    uint8_t dma_request_pending;
    uint8_t dma_request_value;
    uint8_t dma_starting_pending;
    uint8_t dma_starting_value;
    uint8_t dma_active;
    uint8_t dma_source_page;
    int dma_progress; // next OAM offset (0-159) gb_dma_tick() will copy

    // VRAM/WRAM/OAM/I-O-registers/HRAM only as of Phase 2 - 0x0000-0x7FFF
    // (ROM) and 0xA000-0xBFFF (external cartridge RAM) are no longer part
    // of this flat array, routed through `cart` instead. See mmu.c.
    uint8_t *memory;

    // CGB WRAM banking (SVBK, 0xFF70): banks 2-7 of the switchable
    // 0xD000-0xDFFF window - pandocs' Memory_Map.md. Bank 0
    // (0xC000-0xCFFF) and bank 1 (0xD000-0xDFFF's DMG/CGB-bank-1
    // default) stay exactly where they've always been, in `memory`
    // itself - only banks 2-7 need separate storage, mirroring how
    // cart.c's own RAM banking already keeps a separate array indexed by
    // bank number rather than swapping bytes in and out of a flat
    // window. svbk is the raw last-written register value (bits 0-2);
    // mmu.c resolves it to an effective bank (0 reads back as bank 1,
    // a real hardware quirk) at access time. Both are no-ops in DMG mode
    // (is_cgb == 0) - see mmu.c's gb_read_byte/gb_write_byte.
    uint8_t wram_bank[6][0x1000];
    uint8_t svbk;

    // Forward-declared rather than #include "cart.h" here - cpu.h
    // shouldn't need to know GBCart's internals, only that mmu.c can
    // reach one through a GBCpu. NULL is valid (Phase 1's old flat-ROM
    // behavior no longer applies once this is wired up in main.c, but
    // the field itself doesn't require a cart to exist for the struct
    // to be well-formed).
    struct GBCart *cart;

    // Same forward-declaration reasoning as `cart` above, added Phase 3.
    struct GBPpu *ppu;

    // Added Phase 4. `timer` is reached directly (not just through
    // mmu.c's routing) because the STOP instruction's handler needs to
    // reset it exactly as a DIV write would - see cpu.c's gb_op_stop().
    struct GBTimer *timer;
    struct GBJoypad *joypad;

    // Added Phase 5 - reached only through mmu.c's routing (the full
    // 0xFF10-0xFF3F span), unlike timer above.
    struct GBApu *apu;
} GBCpu;

uint8_t gb_read_byte(GBCpu *cpu, uint16_t addr);
void gb_write_byte(GBCpu *cpu, uint16_t addr, uint8_t val);

// Advances OAM DMA's state machine by exactly one M-cycle - see the
// GBCpu struct's own dma_requested/dma_starting/dma_active/
// dma_progress comment above. Called once per real M-cycle of CPU
// execution (cpu.c), not once per instruction, so DMA's progress stays
// correctly interleaved with whatever the CPU is doing that same
// M-cycle - the entire point of this being a per-M-cycle function
// rather than an instant bulk copy.
void gb_dma_tick(GBCpu *cpu);

// Advances DMA, the timer, the PPU, and the APU each by exactly one
// real M-cycle - what every opcode handler in cpu.c actually calls
// once per real M-cycle it takes (gb_dma_tick() above is kept as its
// own function purely because a few direct unit tests, e.g.
// tests/test_cpu.c's test_dma_wram_mirror_source(), exercise DMA in
// isolation without a full GBCpu/GBTimer/GBPpu/GBApu wired together).
// See mmu.c's own comment for the full reasoning.
void gb_mcycle_tick(GBCpu *cpu);

// Unlike Z80OpcodeHandler (cpm/emu/src/z80.h), no separate ram parameter -
// GBCpu carries its own memory pointer and every handler goes through
// gb_read_byte/gb_write_byte, so there's nothing a second parameter
// would add.
typedef int (*GBOpcodeHandler)(GBCpu *cpu);

// Returns the number of T-cycles (4.194304 MHz ticks - not the
// "M-cycles" = T-cycles/4 some references count in) the executed
// instruction took, or a negative value for a genuinely unimplemented
// opcode (the 11 official gaps in the unprefixed table, or a dispatch
// bug), mirroring z80_step()'s own convention in cpm/emu/src/z80.h.
int gb_cpu_step(GBCpu *cpu);

void gb_cpu_init_tables(void);
void gb_cpu_reset(GBCpu *cpu);

#endif
