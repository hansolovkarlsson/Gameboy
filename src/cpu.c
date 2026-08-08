#include "cpu.h"
#include "alu.h"
#include "cart.h"
#include "timer.h"
#include <stddef.h>

// Table-driven dispatch, same *shape* as cpm/emu/src/z80.c's
// main_opcode_table/z80_op_ld_r_r/z80_op_alu_group pattern - see
// docs/GAMEBOY_ROADMAP.md's "Architecture decision" for why this is an
// independent implementation rather than shared code. Every opcode's
// bytes/cycles/flags below were checked against the official gbdev.io
// opcode table (fetched during this phase - see docs/GAMEBOY_ROADMAP.md
// Status section for the exact source and the one real erratum found
// along the way: BIT b,(HL) is 12 cycles, not 16 like the read-modify-
// write CB ops, since it never writes anything back).

GBOpcodeHandler gb_opcode_table[256];

static inline uint8_t fetch_byte(GBCpu *cpu) {
    return gb_read_byte(cpu, cpu->pc++);
}

static inline uint16_t fetch_word(GBCpu *cpu) {
    uint8_t lo = fetch_byte(cpu);
    uint8_t hi = fetch_byte(cpu);
    return (uint16_t)((hi << 8) | lo);
}

// Ticked push/pop - used by every opcode handler whose own real M-cycle
// timing is precise enough to matter for OAM DMA bus-conflict tests
// (CALL/CALL cc/RST/PUSH rr and RET/RET cc/RETI/POP rr - see each
// handler below). One gb_dma_tick() call per real M-cycle these two
// M-cycle memory accesses take, called *before* the matching
// gb_read_byte()/gb_write_byte() so DMA's state is fully up to date
// (possibly just finishing its own transfer, or dropping this exact
// write) by the time that access is evaluated - the same per-M-cycle
// ordering Gekkio's mooneye-gb reference emulator uses (see
// gb_dma_tick()'s own comment, mmu.c).
static void gb_push16_ticked(GBCpu *cpu, uint16_t val) {
    // High byte is written first, low byte second - real hardware's
    // actual M-cycle order for PUSH/CALL/RST/interrupt dispatch alike
    // (Mooneye's push_timing.s/call_timing.s/rst_timing.s, test_roms/
    // mooneye/, each cite this explicitly: "M=2: memory access for
    // high byte, M=3: memory access for low byte"). Observably
    // identical to writing low-then-high in every normal case (same
    // final SP, same final two bytes) - only matters when one of these
    // writes happens to alias a live register with its own side
    // effects (see gb_cpu_step()'s interrupt-dispatch IE-aliasing
    // handling, the one case that currently depends on this order) or,
    // as of this OAM-DMA timing work, when one of the two lands inside
    // an active DMA transfer's bus-conflict window and the other doesn't.
    cpu->sp -= 2;
    gb_dma_tick(cpu);
    gb_write_byte(cpu, (uint16_t)(cpu->sp + 1), val >> 8);
    gb_dma_tick(cpu);
    gb_write_byte(cpu, cpu->sp, val & 0xFF);
}

static uint16_t gb_pop16_ticked(GBCpu *cpu) {
    gb_dma_tick(cpu);
    uint8_t lo = gb_read_byte(cpu, cpu->sp);
    gb_dma_tick(cpu);
    uint8_t hi = gb_read_byte(cpu, (uint16_t)(cpu->sp + 1));
    cpu->sp += 2;
    return (uint16_t)((hi << 8) | lo);
}

// 8-bit register index, shared by every opcode range that encodes one:
// 0=B 1=C 2=D 3=E 4=H 5=L 6=(HL) 7=A - identical convention to the Z80's
// own get_cb_reg/set_cb_reg (cpm/emu/src/z80.c), inherited unchanged since
// this part of the encoding genuinely didn't change between the two CPUs.
static uint8_t get_reg8(GBCpu *cpu, uint8_t idx) {
    switch (idx) {
        case 0: return cpu->b;
        case 1: return cpu->c;
        case 2: return cpu->d;
        case 3: return cpu->e;
        case 4: return cpu->h;
        case 5: return cpu->l;
        case 6: return gb_read_byte(cpu, cpu->hl);
        default: return cpu->a; // 7
    }
}

static void set_reg8(GBCpu *cpu, uint8_t idx, uint8_t val) {
    switch (idx) {
        case 0: cpu->b = val; break;
        case 1: cpu->c = val; break;
        case 2: cpu->d = val; break;
        case 3: cpu->e = val; break;
        case 4: cpu->h = val; break;
        case 5: cpu->l = val; break;
        case 6: gb_write_byte(cpu, cpu->hl, val); break;
        default: cpu->a = val; break; // 7
    }
}

// 16-bit register-pair index used by LD rr,d16 / INC rr / DEC rr /
// ADD HL,rr: 0=BC 1=DE 2=HL 3=SP.
static uint16_t get_rr(GBCpu *cpu, uint8_t idx) {
    switch (idx) {
        case 0: return cpu->bc;
        case 1: return cpu->de;
        case 2: return cpu->hl;
        default: return cpu->sp; // 3
    }
}

static void set_rr(GBCpu *cpu, uint8_t idx, uint16_t val) {
    switch (idx) {
        case 0: cpu->bc = val; break;
        case 1: cpu->de = val; break;
        case 2: cpu->hl = val; break;
        default: cpu->sp = val; break; // 3
    }
}

// The PUSH/POP register-pair index is the same encoding but with AF in
// slot 3 instead of SP - a real, deliberate difference in the opcode
// map (0xF5/0xF1 push/pop AF, never SP), not an inconsistency.
static uint16_t get_rr2(GBCpu *cpu, uint8_t idx) {
    switch (idx) {
        case 0: return cpu->bc;
        case 1: return cpu->de;
        case 2: return cpu->hl;
        default: return cpu->af; // 3
    }
}

static void set_rr2(GBCpu *cpu, uint8_t idx, uint16_t val) {
    switch (idx) {
        case 0: cpu->bc = val; break;
        case 1: cpu->de = val; break;
        case 2: cpu->hl = val; break;
        default: cpu->af = val & 0xFFF0; break; // 3 - F's low nibble always reads 0
    }
}

// Condition-code index used by JR/JP/CALL/RET's conditional forms:
// 0=NZ 1=Z 2=NC 3=C.
static int check_cond(GBCpu *cpu, uint8_t idx) {
    switch (idx) {
        case 0: return !(cpu->f & GB_FLAG_Z);
        case 1: return (cpu->f & GB_FLAG_Z) != 0;
        case 2: return !(cpu->f & GB_FLAG_C);
        default: return (cpu->f & GB_FLAG_C) != 0; // 3
    }
}

static void gb_alu_dispatch(GBCpu *cpu, uint8_t op_idx, uint8_t val) {
    switch (op_idx) {
        case 0: gb_alu_add(cpu, val); break;
        case 1: gb_alu_adc(cpu, val); break;
        case 2: gb_alu_sub(cpu, val); break;
        case 3: gb_alu_sbc(cpu, val); break;
        case 4: gb_alu_and(cpu, val); break;
        case 5: gb_alu_xor(cpu, val); break;
        case 6: gb_alu_or(cpu, val); break;
        default: gb_alu_cp(cpu, val); break; // 7
    }
}

// --- Individually-named opcodes (everything that isn't one of the
// four fully regular blocks below: LD r,r; the r8 and d8 ALU groups;
// and the CB-prefixed table) ---

static int gb_op_nop(GBCpu *cpu) { (void)cpu; return 4; }

static int gb_op_ld_bc_a(GBCpu *cpu) { gb_write_byte(cpu, cpu->bc, cpu->a); return 8; }
static int gb_op_ld_de_a(GBCpu *cpu) { gb_write_byte(cpu, cpu->de, cpu->a); return 8; }
static int gb_op_ld_a_bc(GBCpu *cpu) { cpu->a = gb_read_byte(cpu, cpu->bc); return 8; }
static int gb_op_ld_a_de(GBCpu *cpu) { cpu->a = gb_read_byte(cpu, cpu->de); return 8; }

static int gb_op_ld_hli_a(GBCpu *cpu) { gb_write_byte(cpu, cpu->hl, cpu->a); cpu->hl++; return 8; }
static int gb_op_ld_hld_a(GBCpu *cpu) { gb_write_byte(cpu, cpu->hl, cpu->a); cpu->hl--; return 8; }
static int gb_op_ld_a_hli(GBCpu *cpu) { cpu->a = gb_read_byte(cpu, cpu->hl); cpu->hl++; return 8; }
static int gb_op_ld_a_hld(GBCpu *cpu) { cpu->a = gb_read_byte(cpu, cpu->hl); cpu->hl--; return 8; }

static int gb_op_ld_a16_sp(GBCpu *cpu) {
    uint16_t addr = fetch_word(cpu);
    gb_write_byte(cpu, addr, cpu->sp & 0xFF);
    gb_write_byte(cpu, (uint16_t)(addr + 1), cpu->sp >> 8);
    return 20;
}

static int gb_op_ld_a16_a(GBCpu *cpu) {
    uint16_t addr = fetch_word(cpu);
    gb_write_byte(cpu, addr, cpu->a);
    return 16;
}

static int gb_op_ld_a_a16(GBCpu *cpu) {
    uint16_t addr = fetch_word(cpu);
    cpu->a = gb_read_byte(cpu, addr);
    return 16;
}

// Needs precise per-M-cycle DMA ticking, unlike almost every other
// non-precise/fallback opcode in this file - not because *this*
// instruction's own timing is Mooneye-tested, but because this is the
// specific instruction Mooneye's own start_oam_dma macro (`ldh
// (<DMA), a`) uses to trigger a transfer, and this OAM-DMA-timing
// rewrite's whole model measures the transfer's start relative to the
// exact M-cycle the CPU's write to $FF46 happens on (see gb_dma_tick(),
// mmu.c). Ticking this handler as a single post-hoc lump sum (like
// every other fallback opcode) instead of M1-then-M2 would set
// dma_request_pending 2 M-cycles too early relative to instruction
// boundaries - harmless for the register write itself, but it would
// shift every downstream NOP-padding-based timing test in test_roms/
// mooneye/ by exactly the 2 ticks this fixes.
static int gb_op_ldh_a8_a(GBCpu *cpu) {
    gb_dma_tick(cpu);
    uint8_t off = fetch_byte(cpu);
    gb_dma_tick(cpu);
    gb_write_byte(cpu, (uint16_t)(0xFF00 + off), cpu->a);
    return 12;
}

static int gb_op_ldh_a_a8(GBCpu *cpu) {
    uint8_t off = fetch_byte(cpu);
    cpu->a = gb_read_byte(cpu, (uint16_t)(0xFF00 + off));
    return 12;
}

static int gb_op_ldh_c_a(GBCpu *cpu) { gb_write_byte(cpu, (uint16_t)(0xFF00 + cpu->c), cpu->a); return 8; }
static int gb_op_ldh_a_c(GBCpu *cpu) { cpu->a = gb_read_byte(cpu, (uint16_t)(0xFF00 + cpu->c)); return 8; }

static int gb_op_ld_sp_hl(GBCpu *cpu) { cpu->sp = cpu->hl; return 8; }

// M-cycle breakdown per Mooneye's own add_sp_e_timing.s comment (M=0
// decode, M=1 read e, M=2/M=3 internal) - the caller (gb_cpu_step) has
// already ticked M=0 before dispatch, so this handler only needs to
// tick M=1-M=3 itself.
static int gb_op_add_sp_e8(GBCpu *cpu) {
    gb_dma_tick(cpu);
    int8_t e8 = (int8_t)fetch_byte(cpu);
    gb_dma_tick(cpu);
    gb_dma_tick(cpu);
    cpu->sp = gb_alu_add_sp_e8(cpu, e8);
    return 16;
}

// Same M=0/M=1 as ADD SP,e8 above, but only one internal M=2 cycle
// (Mooneye's ld_hl_sp_e_timing.s) - the load into HL needs one less
// internal cycle than SP itself gets overwritten.
static int gb_op_ld_hl_sp_e8(GBCpu *cpu) {
    gb_dma_tick(cpu);
    int8_t e8 = (int8_t)fetch_byte(cpu);
    gb_dma_tick(cpu);
    cpu->hl = gb_alu_add_sp_e8(cpu, e8);
    return 12;
}

static int gb_op_rlca(GBCpu *cpu) { cpu->a = gb_alu_rlca(cpu, cpu->a); return 4; }
static int gb_op_rrca(GBCpu *cpu) { cpu->a = gb_alu_rrca(cpu, cpu->a); return 4; }
static int gb_op_rla(GBCpu *cpu) { cpu->a = gb_alu_rla(cpu, cpu->a); return 4; }
static int gb_op_rra(GBCpu *cpu) { cpu->a = gb_alu_rra(cpu, cpu->a); return 4; }
static int gb_op_daa(GBCpu *cpu) { gb_alu_daa(cpu); return 4; }
static int gb_op_cpl(GBCpu *cpu) { gb_alu_cpl(cpu); return 4; }
static int gb_op_scf(GBCpu *cpu) { gb_alu_scf(cpu); return 4; }
static int gb_op_ccf(GBCpu *cpu) { gb_alu_ccf(cpu); return 4; }

static int gb_op_jr(GBCpu *cpu) {
    int8_t off = (int8_t)fetch_byte(cpu);
    cpu->pc = (uint16_t)(cpu->pc + off);
    return 12;
}

// M-cycle breakdown per Mooneye's jp_timing.s (M=0 decode, M=1 read
// low byte, M=2 read high byte, M=3 internal delay).
static int gb_op_jp(GBCpu *cpu) {
    gb_dma_tick(cpu);
    uint8_t lo = fetch_byte(cpu);
    gb_dma_tick(cpu);
    uint8_t hi = fetch_byte(cpu);
    gb_dma_tick(cpu);
    cpu->pc = (uint16_t)((hi << 8) | lo);
    return 16;
}

// Real behavior: jump straight to the value *in* HL - unlike every
// other "(HL)" operand in this table, this one is not a memory
// dereference (confirmed against the official opcode table's per-
// operand `immediate` flag - see docs/GAMEBOY_ROADMAP.md).
static int gb_op_jp_hl(GBCpu *cpu) { cpu->pc = cpu->hl; return 4; }

// M-cycle breakdown per Mooneye's call_timing.s (M=0 decode, M=1/M=2
// read nn, M=3 internal, M=4/M=5 PC push).
static int gb_op_call(GBCpu *cpu) {
    gb_dma_tick(cpu);
    uint8_t lo = fetch_byte(cpu);
    gb_dma_tick(cpu);
    uint8_t hi = fetch_byte(cpu);
    gb_dma_tick(cpu);
    uint16_t addr = (uint16_t)((hi << 8) | lo);
    gb_push16_ticked(cpu, cpu->pc);
    cpu->pc = addr;
    return 24;
}

// M-cycle breakdown per Mooneye's ret_timing.s (M=0 decode, M=1/M=2 PC
// pop, M=3 internal).
static int gb_op_ret(GBCpu *cpu) {
    uint16_t addr = gb_pop16_ticked(cpu);
    gb_dma_tick(cpu);
    cpu->pc = addr;
    return 16;
}

// RETI sets IME immediately, unlike EI - there's no one-instruction
// delay here since, unlike EI, there's no risk of it firing before the
// interrupt handler it's returning from has even finished tidying up.
// Same M-cycle breakdown as RET (Mooneye's reti_timing.s).
static int gb_op_reti(GBCpu *cpu) {
    uint16_t addr = gb_pop16_ticked(cpu);
    gb_dma_tick(cpu);
    cpu->pc = addr;
    cpu->ime = 1;
    return 16;
}

static int gb_op_di(GBCpu *cpu) { cpu->ime = 0; cpu->ime_pending = 0; cpu->di_cancels_ei_delay = 1; return 4; }
static int gb_op_ei(GBCpu *cpu) { cpu->ime_pending = 1; return 4; }

// STOP is a real 2-byte instruction (opcode + a padding byte, normally
// 0x00) per the official opcode table, not the 1-byte form some older
// references list - confirmed during Phase 1, not guessed. Resets the
// system counter exactly like a DIV write does (pandocs'
// Timer_and_Divider_Registers.md). Real hardware's full low-power STOP
// mode, and exiting it via a joypad press, needs an actual input
// source to ever trigger - still deferred to Phase 7's real front end,
// since Phase 4 only adds the joypad *register*, not a way to press a
// button from outside the emulator.
static int gb_op_stop(GBCpu *cpu) {
    fetch_byte(cpu);
    cpu->stopped = 1;
    gb_timer_reset_div(cpu->timer, cpu);
    return 4;
}

static int gb_op_illegal(GBCpu *cpu) { (void)cpu; return -1; }

// --- Fully regular blocks: one handler each, decoding the actual
// opcode byte out of cpu->pc-1 the same way z80_op_ld_r_r does
// (cpm/emu/src/z80.c) ---

static int gb_op_ld_rr_d16(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 4) & 0x03;
    set_rr(cpu, idx, fetch_word(cpu));
    return 12;
}

static int gb_op_inc_rr(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 4) & 0x03;
    set_rr(cpu, idx, (uint16_t)(get_rr(cpu, idx) + 1));
    return 8;
}

static int gb_op_dec_rr(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 4) & 0x03;
    set_rr(cpu, idx, (uint16_t)(get_rr(cpu, idx) - 1));
    return 8;
}

static int gb_op_add_hl_rr(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 4) & 0x03;
    gb_alu_add_hl(cpu, get_rr(cpu, idx));
    return 8;
}

// M-cycle breakdown per Mooneye's push_timing.s (M=0 decode, M=1
// internal, M=2/M=3 write).
static int gb_op_push_rr2(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 4) & 0x03;
    gb_dma_tick(cpu);
    gb_push16_ticked(cpu, get_rr2(cpu, idx));
    return 16;
}

// M-cycle breakdown per Mooneye's pop_timing.s (M=0 decode, M=1/M=2
// read) - notably no internal delay cycle, unlike PUSH above.
static int gb_op_pop_rr2(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 4) & 0x03;
    set_rr2(cpu, idx, gb_pop16_ticked(cpu));
    return 12;
}

static int gb_op_inc_r(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 3) & 0x07;
    set_reg8(cpu, idx, gb_alu_inc(cpu, get_reg8(cpu, idx)));
    return (idx == 6) ? 12 : 4;
}

static int gb_op_dec_r(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 3) & 0x07;
    set_reg8(cpu, idx, gb_alu_dec(cpu, get_reg8(cpu, idx)));
    return (idx == 6) ? 12 : 4;
}

static int gb_op_ld_r_d8(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 3) & 0x07;
    uint8_t val = fetch_byte(cpu);
    set_reg8(cpu, idx, val);
    return (idx == 6) ? 12 : 8;
}

// Covers 0x40-0x7F. 0x76 (which would otherwise decode as the
// impossible "LD (HL),(HL)") is HALT instead - same real hardware
// special case the Z80 shares, handled the same way z80_op_ld_r_r does.
static int gb_op_ld_r_r(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    if (opcode == 0x76) {
        uint8_t pending = (uint8_t)(gb_read_byte(cpu, 0xFFFF) & gb_read_byte(cpu, 0xFF0F) & 0x1F);
        if (!cpu->ime && pending && cpu->ei_delay_active) {
            // pandocs' halt.md, the "halt immediately after ei" sub-case:
            // real hardware doesn't apply the generic halt-bug double-
            // fetch here. Instead, HALT is effectively canceled outright
            // (pc rewound back to HALT's own address, no halt_bug, no
            // halted) - by the *next* gb_cpu_step call, ime_pending's
            // one-instruction delay (gb_cpu_step's own ime_to_set/
            // cpu->ime_pending handling) has resolved to ime=1, so the
            // already-pending interrupt dispatches completely normally
            // next step, using this now-correct, unadvanced pc as its
            // return address - meaning RETI naturally resumes execution
            // back at this same HALT, which by then sees ime=1 for real
            // and halts properly ("waits for another interrupt", per
            // pandocs). Found via a real ROM (test_roms/tobutobugirl/)
            // whose main loop's own "ei; halt" idiom hit exactly this:
            // treating it as the generic halt_bug case instead pushed
            // the wrong return address (pc *after* HALT, not pc *at*
            // HALT) when the interrupt fired, and separately left
            // halt_bug=1 to incorrectly fire again on the interrupt
            // vector's own first instruction once inside the handler,
            // double-executing it and corrupting the stack by 2 bytes -
            // see test_roms/tobutobugirl/README.md for the full story.
            cpu->pc--;
        } else if (!cpu->ime && pending) {
            // The generic HALT bug (pandocs' halt.md): IME=0 with an
            // interrupt already pending means HALT doesn't actually
            // halt at all - see gb_cpu_step()'s own handling of
            // halt_bug for what this actually does to the next
            // instruction. Distinct from the ei-delay sub-case above,
            // which pandocs documents as behaving differently.
            cpu->halt_bug = 1;
        } else {
            cpu->halted = 1;
        }
        return 4;
    }
    uint8_t dst_idx = (opcode >> 3) & 0x07;
    uint8_t src_idx = opcode & 0x07;
    set_reg8(cpu, dst_idx, get_reg8(cpu, src_idx));
    return (dst_idx == 6 || src_idx == 6) ? 8 : 4;
}

// Covers 0x80-0xBF: ADD/ADC/SUB/SBC/AND/XOR/OR/CP against r8.
static int gb_op_alu_group(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t op_idx = (opcode >> 3) & 0x07;
    uint8_t reg_idx = opcode & 0x07;
    gb_alu_dispatch(cpu, op_idx, get_reg8(cpu, reg_idx));
    return (reg_idx == 6) ? 8 : 4;
}

// Covers the 8 ALU-against-immediate opcodes (0xC6/CE/D6/DE/E6/EE/F6/FE),
// spaced by 0x08 the same way the r8 group above is spaced by 0x01.
static int gb_op_alu_d8_group(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t op_idx = (opcode >> 3) & 0x07;
    gb_alu_dispatch(cpu, op_idx, fetch_byte(cpu));
    return 8;
}

static int gb_op_jr_cc(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 3) & 0x03;
    int8_t off = (int8_t)fetch_byte(cpu);
    if (check_cond(cpu, idx)) {
        cpu->pc = (uint16_t)(cpu->pc + off);
        return 12;
    }
    return 8;
}

// M-cycle breakdown per Mooneye's jp_cc_timing.s (M=0 decode, M=1/M=2
// read nn, M=3 internal *only if taken* - the not-taken case skips it,
// matching JP cc's real 12T/3M vs 16T/4M split).
static int gb_op_jp_cc(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 3) & 0x03;
    gb_dma_tick(cpu);
    uint8_t lo = fetch_byte(cpu);
    gb_dma_tick(cpu);
    uint8_t hi = fetch_byte(cpu);
    uint16_t addr = (uint16_t)((hi << 8) | lo);
    if (check_cond(cpu, idx)) {
        gb_dma_tick(cpu);
        cpu->pc = addr;
        return 16;
    }
    return 12;
}

// M-cycle breakdown per Mooneye's call_cc_timing.s/call_cc_timing2.s
// (M=0 decode, M=1/M=2 read nn, M=3 internal + M=4/M=5 push - all only
// if taken; the not-taken case stops after M=2, matching CALL cc's
// real 12T/3M vs 24T/6M split).
static int gb_op_call_cc(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 3) & 0x03;
    gb_dma_tick(cpu);
    uint8_t lo = fetch_byte(cpu);
    gb_dma_tick(cpu);
    uint8_t hi = fetch_byte(cpu);
    uint16_t addr = (uint16_t)((hi << 8) | lo);
    if (check_cond(cpu, idx)) {
        gb_dma_tick(cpu);
        gb_push16_ticked(cpu, cpu->pc);
        cpu->pc = addr;
        return 24;
    }
    return 12;
}

// M-cycle breakdown per Mooneye's ret_cc_timing.s (M=0 decode, M=1
// internal *always* - the condition check itself costs a cycle even
// when not taken, unlike JP cc/CALL cc above - M=2/M=3 pop + M=4
// internal only if taken).
static int gb_op_ret_cc(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint8_t idx = (opcode >> 3) & 0x03;
    gb_dma_tick(cpu);
    if (check_cond(cpu, idx)) {
        uint16_t addr = gb_pop16_ticked(cpu);
        gb_dma_tick(cpu);
        cpu->pc = addr;
        return 20;
    }
    return 8;
}

// M-cycle breakdown per Mooneye's rst_timing.s (M=0 decode, M=1
// internal, M=2/M=3 push) - identical shape to PUSH rr2 above, just
// with a fixed target instead of a register-pair value.
static int gb_op_rst(GBCpu *cpu) {
    uint8_t opcode = gb_read_byte(cpu, (uint16_t)(cpu->pc - 1));
    uint16_t target = opcode & 0x38;
    gb_dma_tick(cpu);
    gb_push16_ticked(cpu, cpu->pc);
    cpu->pc = target;
    return 16;
}

// The entire CB-prefixed table (0xCB is a single main_opcode_table-
// style entry, not a 256-entry sub-table - same choice CLAUDE.md
// documents z80_op_prefix_cb making). Fully regular on the SM83 (no
// IX/IY-driven exceptions to carve out the way the Z80's version has
// to): bits 7-6 select the operation group, bits 5-3 select which
// rotate/shift/bit-index, bits 2-0 select the r8 operand.
static int gb_op_prefix_cb(GBCpu *cpu) {
    uint8_t opcode = fetch_byte(cpu);
    uint8_t reg_idx = opcode & 0x07;
    uint8_t bit_idx = (opcode >> 3) & 0x07;
    uint8_t group = (opcode >> 6) & 0x03;
    uint8_t val = get_reg8(cpu, reg_idx);

    if (group == 0) {
        uint8_t result;
        switch (bit_idx) {
            case 0: result = gb_alu_rlc(cpu, val); break;
            case 1: result = gb_alu_rrc(cpu, val); break;
            case 2: result = gb_alu_rl(cpu, val); break;
            case 3: result = gb_alu_rr(cpu, val); break;
            case 4: result = gb_alu_sla(cpu, val); break;
            case 5: result = gb_alu_sra(cpu, val); break;
            case 6: result = gb_alu_swap(cpu, val); break;
            default: result = gb_alu_srl(cpu, val); break; // 7
        }
        set_reg8(cpu, reg_idx, result);
        return (reg_idx == 6) ? 16 : 8;
    }
    if (group == 1) {
        gb_alu_bit(cpu, bit_idx, val);
        // BIT never writes anything back, so the (HL) form skips the
        // write-back cycles the other three groups need - 12, not 16.
        // Confirmed against the official gbdev.io opcode table; a
        // commonly-mirrored community JSON dataset gets this specific
        // case wrong (says 16) - see docs/GAMEBOY_ROADMAP.md.
        return (reg_idx == 6) ? 12 : 8;
    }
    if (group == 2) {
        set_reg8(cpu, reg_idx, gb_alu_res(bit_idx, val));
    } else {
        set_reg8(cpu, reg_idx, gb_alu_set(bit_idx, val));
    }
    return (reg_idx == 6) ? 16 : 8;
}

void gb_cpu_init_tables(void) {
    for (int i = 0; i < 256; i++) gb_opcode_table[i] = gb_op_illegal;

    gb_opcode_table[0x00] = gb_op_nop;
    gb_opcode_table[0x01] = gb_op_ld_rr_d16;
    gb_opcode_table[0x02] = gb_op_ld_bc_a;
    gb_opcode_table[0x03] = gb_op_inc_rr;
    gb_opcode_table[0x04] = gb_op_inc_r;
    gb_opcode_table[0x05] = gb_op_dec_r;
    gb_opcode_table[0x06] = gb_op_ld_r_d8;
    gb_opcode_table[0x07] = gb_op_rlca;
    gb_opcode_table[0x08] = gb_op_ld_a16_sp;
    gb_opcode_table[0x09] = gb_op_add_hl_rr;
    gb_opcode_table[0x0A] = gb_op_ld_a_bc;
    gb_opcode_table[0x0B] = gb_op_dec_rr;
    gb_opcode_table[0x0C] = gb_op_inc_r;
    gb_opcode_table[0x0D] = gb_op_dec_r;
    gb_opcode_table[0x0E] = gb_op_ld_r_d8;
    gb_opcode_table[0x0F] = gb_op_rrca;

    gb_opcode_table[0x10] = gb_op_stop;
    gb_opcode_table[0x11] = gb_op_ld_rr_d16;
    gb_opcode_table[0x12] = gb_op_ld_de_a;
    gb_opcode_table[0x13] = gb_op_inc_rr;
    gb_opcode_table[0x14] = gb_op_inc_r;
    gb_opcode_table[0x15] = gb_op_dec_r;
    gb_opcode_table[0x16] = gb_op_ld_r_d8;
    gb_opcode_table[0x17] = gb_op_rla;
    gb_opcode_table[0x18] = gb_op_jr;
    gb_opcode_table[0x19] = gb_op_add_hl_rr;
    gb_opcode_table[0x1A] = gb_op_ld_a_de;
    gb_opcode_table[0x1B] = gb_op_dec_rr;
    gb_opcode_table[0x1C] = gb_op_inc_r;
    gb_opcode_table[0x1D] = gb_op_dec_r;
    gb_opcode_table[0x1E] = gb_op_ld_r_d8;
    gb_opcode_table[0x1F] = gb_op_rra;

    gb_opcode_table[0x20] = gb_op_jr_cc;
    gb_opcode_table[0x21] = gb_op_ld_rr_d16;
    gb_opcode_table[0x22] = gb_op_ld_hli_a;
    gb_opcode_table[0x23] = gb_op_inc_rr;
    gb_opcode_table[0x24] = gb_op_inc_r;
    gb_opcode_table[0x25] = gb_op_dec_r;
    gb_opcode_table[0x26] = gb_op_ld_r_d8;
    gb_opcode_table[0x27] = gb_op_daa;
    gb_opcode_table[0x28] = gb_op_jr_cc;
    gb_opcode_table[0x29] = gb_op_add_hl_rr;
    gb_opcode_table[0x2A] = gb_op_ld_a_hli;
    gb_opcode_table[0x2B] = gb_op_dec_rr;
    gb_opcode_table[0x2C] = gb_op_inc_r;
    gb_opcode_table[0x2D] = gb_op_dec_r;
    gb_opcode_table[0x2E] = gb_op_ld_r_d8;
    gb_opcode_table[0x2F] = gb_op_cpl;

    gb_opcode_table[0x30] = gb_op_jr_cc;
    gb_opcode_table[0x31] = gb_op_ld_rr_d16;
    gb_opcode_table[0x32] = gb_op_ld_hld_a;
    gb_opcode_table[0x33] = gb_op_inc_rr;
    gb_opcode_table[0x34] = gb_op_inc_r;
    gb_opcode_table[0x35] = gb_op_dec_r;
    gb_opcode_table[0x36] = gb_op_ld_r_d8;
    gb_opcode_table[0x37] = gb_op_scf;
    gb_opcode_table[0x38] = gb_op_jr_cc;
    gb_opcode_table[0x39] = gb_op_add_hl_rr;
    gb_opcode_table[0x3A] = gb_op_ld_a_hld;
    gb_opcode_table[0x3B] = gb_op_dec_rr;
    gb_opcode_table[0x3C] = gb_op_inc_r;
    gb_opcode_table[0x3D] = gb_op_dec_r;
    gb_opcode_table[0x3E] = gb_op_ld_r_d8;
    gb_opcode_table[0x3F] = gb_op_ccf;

    for (int i = 0x40; i <= 0x7F; i++) gb_opcode_table[i] = gb_op_ld_r_r;
    for (int i = 0x80; i <= 0xBF; i++) gb_opcode_table[i] = gb_op_alu_group;

    gb_opcode_table[0xC0] = gb_op_ret_cc;
    gb_opcode_table[0xC1] = gb_op_pop_rr2;
    gb_opcode_table[0xC2] = gb_op_jp_cc;
    gb_opcode_table[0xC3] = gb_op_jp;
    gb_opcode_table[0xC4] = gb_op_call_cc;
    gb_opcode_table[0xC5] = gb_op_push_rr2;
    gb_opcode_table[0xC6] = gb_op_alu_d8_group;
    gb_opcode_table[0xC7] = gb_op_rst;
    gb_opcode_table[0xC8] = gb_op_ret_cc;
    gb_opcode_table[0xC9] = gb_op_ret;
    gb_opcode_table[0xCA] = gb_op_jp_cc;
    gb_opcode_table[0xCB] = gb_op_prefix_cb;
    gb_opcode_table[0xCC] = gb_op_call_cc;
    gb_opcode_table[0xCD] = gb_op_call;
    gb_opcode_table[0xCE] = gb_op_alu_d8_group;
    gb_opcode_table[0xCF] = gb_op_rst;

    gb_opcode_table[0xD0] = gb_op_ret_cc;
    gb_opcode_table[0xD1] = gb_op_pop_rr2;
    gb_opcode_table[0xD2] = gb_op_jp_cc;
    gb_opcode_table[0xD4] = gb_op_call_cc;
    gb_opcode_table[0xD5] = gb_op_push_rr2;
    gb_opcode_table[0xD6] = gb_op_alu_d8_group;
    gb_opcode_table[0xD7] = gb_op_rst;
    gb_opcode_table[0xD8] = gb_op_ret_cc;
    gb_opcode_table[0xD9] = gb_op_reti;
    gb_opcode_table[0xDA] = gb_op_jp_cc;
    gb_opcode_table[0xDC] = gb_op_call_cc;
    gb_opcode_table[0xDE] = gb_op_alu_d8_group;
    gb_opcode_table[0xDF] = gb_op_rst;

    gb_opcode_table[0xE0] = gb_op_ldh_a8_a;
    gb_opcode_table[0xE1] = gb_op_pop_rr2;
    gb_opcode_table[0xE2] = gb_op_ldh_c_a;
    gb_opcode_table[0xE5] = gb_op_push_rr2;
    gb_opcode_table[0xE6] = gb_op_alu_d8_group;
    gb_opcode_table[0xE7] = gb_op_rst;
    gb_opcode_table[0xE8] = gb_op_add_sp_e8;
    gb_opcode_table[0xE9] = gb_op_jp_hl;
    gb_opcode_table[0xEA] = gb_op_ld_a16_a;
    gb_opcode_table[0xEE] = gb_op_alu_d8_group;
    gb_opcode_table[0xEF] = gb_op_rst;

    gb_opcode_table[0xF0] = gb_op_ldh_a_a8;
    gb_opcode_table[0xF1] = gb_op_pop_rr2;
    gb_opcode_table[0xF2] = gb_op_ldh_a_c;
    gb_opcode_table[0xF3] = gb_op_di;
    gb_opcode_table[0xF5] = gb_op_push_rr2;
    gb_opcode_table[0xF6] = gb_op_alu_d8_group;
    gb_opcode_table[0xF7] = gb_op_rst;
    gb_opcode_table[0xF8] = gb_op_ld_hl_sp_e8;
    gb_opcode_table[0xF9] = gb_op_ld_sp_hl;
    gb_opcode_table[0xFA] = gb_op_ld_a_a16;
    gb_opcode_table[0xFB] = gb_op_ei;
    gb_opcode_table[0xFE] = gb_op_alu_d8_group;
    gb_opcode_table[0xFF] = gb_op_rst;

    // 0xD3/0xDB/0xDD/0xE3/0xE4/0xEB/0xEC/0xED/0xF4/0xFC/0xFD are left as
    // gb_op_illegal from the loop above - the 11 real gaps in the SM83's
    // opcode map (confirmed against the official table: each is labeled
    // ILLEGAL_xx there, not just "absent" the way an incomplete table
    // would leave them). Real hardware locks up executing one of these;
    // returning -1 here matches z80_op_unimplemented's own convention
    // for "this is a genuine bug in whatever's running", not a gap in
    // this emulator.
}

// Real DMG post-boot-ROM register values (what the Nintendo logo boot
// ROM leaves behind at PC=0x0100, before cartridge code ever runs) -
// fetched from pandocs' Power-Up Sequence page during Phase 1, not
// guessed, since Blargg's test ROMs (and most real games) are written
// assuming this exact starting state. The F register's H/C bits
// genuinely depend on whether the cartridge header checksum byte at
// 0x014D is exactly zero (clear if so, set otherwise - a real, if
// obscure, hardware behavior straight from pandocs' own footnote, not
// about whether the checksum *validates*: a real Game Boy would refuse
// to run the cartridge at all if it didn't). cart is optional - Phase 1
// test binaries with no attached cartridge, or a NULL cart during unit
// testing, still get a sensible default (the far more common "nonzero
// checksum" case).
void gb_cpu_reset(GBCpu *cpu) {
    uint8_t checksum_byte = cpu->cart ? cpu->cart->header_checksum_byte : 0xFF;
    cpu->af = 0x0100 | (checksum_byte == 0 ? 0x80 : 0xB0);
    cpu->bc = 0x0013;
    cpu->de = 0x00D8;
    cpu->hl = 0x014D;
    cpu->sp = 0xFFFE;
    cpu->pc = 0x0100;
    cpu->ime = 0;
    cpu->ime_pending = 0;
    cpu->ei_delay_active = 0;
    cpu->di_cancels_ei_delay = 0;
    cpu->halted = 0;
    cpu->stopped = 0;
    cpu->halt_bug = 0;
    cpu->dma_request_pending = 0;
    cpu->dma_starting_pending = 0;
    cpu->dma_active = 0;
    cpu->dma_source_page = 0;
    cpu->dma_progress = 0;
}

// The five interrupt vectors, indexed by IE/IF bit position - pandocs'
// Interrupts.md: priority follows bit order too, bit 0 (VBlank)
// highest, bit 4 (Joypad) lowest, so scanning from bit 0 upward and
// taking the first set bit is both "find a pending interrupt" and
// "find the highest-priority one" in the same pass.
static const uint16_t interrupt_vectors[5] = {0x0040, 0x0048, 0x0050, 0x0058, 0x0060};

// Fetches and dispatches one opcode, ticking OAM DMA by exactly one
// M-cycle per real M-cycle this instruction takes - the M0 (opcode
// fetch) tick happens right here, once, for every instruction; what
// happens for the rest of the instruction's M-cycles depends on which
// handler this opcode maps to:
//
// - The dozen opcodes whose own real M-cycle boundaries this project's
//   Mooneye OAM-DMA-timing ROMs actually probe (CALL/CALL cc/RET/RET
//   cc/RETI/RST/PUSH rr/POP rr/JP nn/JP cc/ADD SP,e8/LD HL,SP+e8 - see
//   each handler's own comment for its exact M-cycle breakdown), plus
//   LDH (a8),A (gb_op_ldh_a8_a - see its own comment on why *it*, of
//   all things, needs this too: it's the exact instruction that
//   triggers a transfer in the first place), call gb_dma_tick()
//   themselves, once per remaining M-cycle, interleaved with their own
//   reads/writes/internal delays. is_dma_precise_op() below recognizes
//   them by function pointer so this dispatcher knows to leave them
//   alone.
// - Every other opcode doesn't self-tick at all - this function just
//   advances DMA by whatever's left (`cycles/4 - 1`, the "-1" for M0
//   already ticked above) in one lump sum once the handler returns.
//   This is deliberately *not* per-M-cycle-accurate for these opcodes'
//   own internal reads/writes against DMA - but nothing needs it to be:
//   real code never touches OAM directly during an active transfer (it
//   busy-waits in HRAM instead, the same convention this project's PPU
//   code already relies elsewhere), and none of the Mooneye ROMs here
//   probe any other opcode's timing against DMA. What *does* need to be
//   right is DMA's total progress after N real M-cycles of ordinary
//   code (e.g. the NOP-padding loops these same ROMs use to line up
//   their *own* precise instruction against DMA's countdown) - and a
//   same-total lump sum gets that exactly right.
static int is_dma_precise_op(GBOpcodeHandler handler) {
    return handler == gb_op_jp || handler == gb_op_jp_cc ||
           handler == gb_op_call || handler == gb_op_call_cc ||
           handler == gb_op_ret || handler == gb_op_ret_cc || handler == gb_op_reti ||
           handler == gb_op_rst || handler == gb_op_push_rr2 || handler == gb_op_pop_rr2 ||
           handler == gb_op_add_sp_e8 || handler == gb_op_ld_hl_sp_e8 ||
           handler == gb_op_ldh_a8_a;
}

static int fetch_and_dispatch_ticked(GBCpu *cpu) {
    gb_dma_tick(cpu); // M0: opcode fetch
    uint8_t opcode = fetch_byte(cpu);
    GBOpcodeHandler handler = gb_opcode_table[opcode];
    int cycles = handler(cpu);
    if (cycles > 0 && !is_dma_precise_op(handler)) {
        for (int remaining = cycles / 4 - 1; remaining > 0; remaining--) {
            gb_dma_tick(cpu);
        }
    }
    return cycles;
}

int gb_cpu_step(GBCpu *cpu) {
    uint8_t pending = (uint8_t)(gb_read_byte(cpu, 0xFFFF) & gb_read_byte(cpu, 0xFF0F) & 0x1F);

    // Any pending, enabled interrupt wakes the CPU from HALT even when
    // IME=0 (pandocs' halt.md: "if no interrupt is pending, halt
    // executes as normal, and the CPU resumes regular execution as
    // soon as an interrupt becomes pending" - IME only gates whether
    // the handler actually runs, not whether HALT itself ends).
    if (cpu->halted && pending) cpu->halted = 0;

    if (cpu->ime && pending) {
        // pandocs' Interrupts.md "Interrupt handling": IME is cleared,
        // then this behaves like a CALL to the vector - 5 M-cycles (20
        // T-states) total. Which vector, though, isn't locked in until
        // right after the PC push's *high*-byte write specifically -
        // not before either write, and not after the low-byte write
        // either. If SP happens to be $0000/$0001 at dispatch time,
        // that high-byte write lands on IE ($FFFF) itself; a value
        // clobbering away the triggering bit at that exact moment
        // genuinely cancels the interrupt (PC ends up at $0000, IF's
        // bit is never cleared), while the same clobber happening only
        // on the low-byte write is real but always too late to change
        // anything - the vector's already committed to by then.
        // Verified against all four rounds of Mooneye's real-hardware-
        // verified interrupts/ie_push.gb (test_roms/mooneye/), which
        // exercises exactly this: normal dispatch, IE-clobber-cancels,
        // IE-clobber-too-late, and a two-candidate-interrupts case
        // proving the *fresh* IE (not the original) picks which one.
        // Not one of Mooneye's precisely-probed opcodes (none of this
        // project's committed ROMs test interrupt dispatch racing an
        // active OAM DMA transfer), so these 5 ticks - one per real
        // M-cycle, matching pandocs' own 5-M-cycle dispatch sequence -
        // are placed at reasonable points rather than exhaustively
        // verified against a real ROM the way the 12 opcode handlers
        // above are.
        gb_dma_tick(cpu); // M0: decode/IME clear
        cpu->ime = 0;
        cpu->sp -= 2;
        gb_dma_tick(cpu); // M1: high byte push
        gb_write_byte(cpu, (uint16_t)(cpu->sp + 1), (uint8_t)(cpu->pc >> 8));

        gb_dma_tick(cpu); // M2: internal (decide the target vector)
        uint8_t final_pending = (uint8_t)(gb_read_byte(cpu, 0xFFFF) & gb_read_byte(cpu, 0xFF0F) & 0x1F);
        uint16_t target = 0x0000;
        if (final_pending) {
            int bit = 0;
            while (!(final_pending & (1 << bit))) bit++;
            gb_write_byte(cpu, 0xFF0F, (uint8_t)(gb_read_byte(cpu, 0xFF0F) & ~(1 << bit)));
            target = interrupt_vectors[bit];
        }

        gb_dma_tick(cpu); // M3: low byte push
        gb_write_byte(cpu, cpu->sp, (uint8_t)(cpu->pc & 0xFF));
        gb_dma_tick(cpu); // M4: internal
        cpu->pc = target;
        return 20;
    }

    if (cpu->halted) {
        gb_dma_tick(cpu);
        return 4; // still waiting - no enabled interrupt pending yet
    }

    if (cpu->halt_bug) {
        // The HALT bug (see gb_op_ld_r_r's 0x76 case and pandocs'
        // halt.md): PC fails to advance past the instruction
        // immediately after HALT, so it executes fully now (real side
        // effects and all - the classic "instruction after HALT runs
        // twice" is genuine, not just a refetch), then PC is rewound so
        // the *next* gb_cpu_step() call executes it again for real,
        // this time advancing normally afterward.
        cpu->halt_bug = 0;
        uint16_t start_pc = cpu->pc;
        int cycles = fetch_and_dispatch_ticked(cpu);
        cpu->pc = start_pc;
        return cycles;
    }

    // EI's enable takes effect only after the instruction *following*
    // EI has fully executed - captured here (before this step's fetch)
    // and applied at the bottom (after this step's execute), so it's
    // the instruction after EI, not EI's own step, that's affected.
    // Unless that following instruction is DI itself: real hardware
    // never lets interrupts turn on even momentarily in that case
    // ("ei; di" - rapid or otherwise - never enables interrupts), so
    // gb_op_di() cancels this same delayed enable via
    // di_cancels_ei_delay rather than this code unconditionally
    // re-applying it after DI already set ime back to 0.
    uint8_t ime_to_set = cpu->ime_pending;
    cpu->ime_pending = 0;
    cpu->ei_delay_active = ime_to_set;
    cpu->di_cancels_ei_delay = 0;

    int cycles = fetch_and_dispatch_ticked(cpu);

    if (ime_to_set && !cpu->di_cancels_ei_delay) cpu->ime = 1;

    return cycles;
}
