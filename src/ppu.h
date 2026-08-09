#ifndef _GB_PPU_H
#define _GB_PPU_H

#include <stdint.h>

#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144

struct GBCpu; // forward-declared - see cpu.h's own comment on why

// Phase 3: the LCD controller. Registers (0xFF40-0xFF4B), the mode/
// timing state machine, and a scanline-at-a-time renderer (background,
// window, objects). Every register layout, addressing mode, and
// priority rule below is grounded against pandocs' LCDC.md/STAT.md/
// Tile_Data.md/Tile_Maps.md/OAM.md/Rendering.md/Palettes.md/
// OAM_DMA_Transfer.md (fetched during this phase - see
// docs/GAMEBOY_ROADMAP.md), not guessed.
//
// Phase 8 (see the roadmap's own status entry) replaced Mode 3's fixed
// 172-dot length with pandocs' Rendering.md "Mode 3 length" algorithm -
// SCX%8 + a flat 6-dot window penalty + a per-object penalty (see
// compute_mode3_length() in ppu.c) - closing the STAT-interrupt-timing
// gap that caused dmg-acid2's real, previously-documented flicker.
// Deliberately still *not* a full per-dot pixel-FIFO simulation
// (pixel_fifo.md's fetcher/FIFO push-pop state machine): pandocs'
// Rendering.md algorithm gives Mode 3's exact *duration* without
// needing one, and duration (not literal per-pixel FIFO mixing) is
// what STAT/OAM-scan interrupt timing actually depends on - render_scanline()
// still computes all 160 pixels' final color at once, using the same
// per-scanline register/OAM snapshot compute_mode3_length() used at
// the Mode 2->3 transition. This means genuinely obscure *mid-Mode-3*
// raster tricks (a register write timed to land between two pixels
// within the same scanline, or pixel_fifo.md's own documented "WX
// changed mid-scanline" bug) still aren't modeled - those need the
// full FIFO simulation this phase deliberately didn't build.
typedef struct GBPpu {
    uint8_t lcdc, stat, scy, scx, ly, lyc, dma, bgp, obp0, obp1, wy, wx;

    int mode;         // 0=HBlank 1=VBlank 2=OAM scan 3=Drawing
    int dots;         // dot (T-cycle) counter within the current scanline
    int mode3_dots;   // this scanline's real Mode 3 length, computed once at
                       // the Mode 2->3 transition - see compute_mode3_length()
    int mode3_had_obj; // whether that computation actually fetched >=1 OBJ -
                        // see gb_ppu_step()'s own comment on why the Mode
                        // 3->0 transition needs this to pick its comparator
    int lcd_starting;  // line 0 immediately after LCD is enabled is a real
                        // hardware quirk, distinct from every other line:
                        // no Mode 2 (OAM scan) at all - it starts directly
                        // in Mode 0 for a short, fixed 76-dot window, then
                        // goes straight to Mode 3. See gb_ppu_step()'s own
                        // comment (the mode-0 case) for the full derivation.
    uint8_t stat_line; // the shared internal STAT interrupt line's last-known
                        // level (pandocs' Interrupt_Sources.md "INT $48") -
                        // see ppu.c's own update_stat_line() comment for why
                        // this needs to persist across calls, not just be
                        // recomputed fresh each time
    int visible_mode;  // what gb_ppu_read_reg() reports as STAT's mode bits -
                        // deliberately lags `mode` by exactly one M-cycle; see
                        // gb_ppu_step()'s own comment for why a same-instant
                        // register read needs this distinction from the
                        // internal mode value interrupts key off of
    uint8_t visible_lyc_flag; // what gb_ppu_read_reg() reports as STAT bit 2
                        // (LY==LYC) - the same one-M-cycle read lag as
                        // visible_mode, snapshotted alongside it. Found via
                        // Mooneye's own lcdon_timing-GS.gb
                        // (test_roms/mooneye/): its STAT-with-LYC=1 table
                        // has a read landing on the exact M-cycle LY
                        // increments to 1 (matching LYC) that still
                        // reports the flag clear - the *live* ppu->stat
                        // bit 2 (used until this fix) would already show
                        // it set, since LY itself has no such lag.
    // OAM/VRAM bus-conflict blocking is genuinely *different* for CPU
    // reads vs. writes right at the Mode 2->3 boundary, and different
    // again for OAM vs. VRAM - not the same signal reused four ways.
    // OAM reads see a glitch at the LY-increment M-cycle (forced
    // blocked there even though mode bits read 0); OAM writes instead
    // see an early one-M-cycle *unblock* right before Mode 3 becomes
    // STAT-visible. VRAM reads see the mirror image - an early
    // one-M-cycle *block* at that same boundary; VRAM writes are
    // unaffected and just follow the plain Mode 3 rule. All four found
    // via Mooneye's own lcdon_timing-GS.gb (reads) and
    // lcdon_write_timing-GS.gb (writes) (test_roms/mooneye/) disagreeing
    // at the exact same M-cycles - see gb_ppu_step()'s own comment on
    // ly_about_to_change and mode2_bus_handoff for the two mechanisms,
    // and gb_ppu_oam_blocked()/gb_ppu_vram_blocked()'s own comments for
    // which of the two each field/direction uses.
    int visible_oam_read_blocked;
    int visible_oam_write_blocked;
    int visible_vram_read_blocked;
    int visible_vram_write_blocked;
    int window_line;  // internal window line counter - see Tile_Maps.md's
                       // "Window Internal Line Counter" tip: only advances
                       // on scanlines where the window was actually drawn

    // One byte per pixel: the final DMG shade (0=white .. 3=black),
    // already palette-translated - not the raw tile color index. Good
    // enough for a pixel-for-pixel comparison against a reference image
    // (this phase's actual correctness gate); real RGB/display output
    // is a front-end's job, not modeled until Phase 7.
    //
    // Deliberately kept as its own array, separate from cgb_framebuffer
    // below, rather than unifying both modes through one RGB
    // representation: real DMG hardware has no RGB concept at all, and
    // (verified against tests/compare_frame.py's exact-byte-equality
    // gate against dmg-acid2's own externally-authored reference PNG,
    // and the 2048-gb/tobu_tobu_girl byte-exact frame captures) no
    // 5-bit-RGB555-then-back-to-8-bit round trip can reproduce this
    // array's existing four gray levels (255/170/85/0) exactly - 256
    // isn't evenly divisible into 32 the way it is into 4. Unifying the
    // two would have silently drifted every existing DMG regression
    // reference by a few gray levels, an artifact of the shared pipeline
    // rather than any real rendering change. CGB mode writes
    // cgb_framebuffer only; DMG mode writes framebuffer only - see
    // render_scanline()'s own is_cgb branch (ppu.c).
    uint8_t framebuffer[GB_SCREEN_HEIGHT][GB_SCREEN_WIDTH];

    // CGB mode's real per-pixel color, packed RGB555 (pandocs'
    // Palettes.md: "a single color is composed of three 5-bit
    // components"; bit 15 unused, conventionally clear). Written only
    // when cpu->is_cgb; a front end picks whichever of the two
    // framebuffers to read based on that same flag.
    uint16_t cgb_framebuffer[GB_SCREEN_HEIGHT][GB_SCREEN_WIDTH];

    // CGB VRAM banking (VBK, 0xFF4F): bank 0 stays exactly where it's
    // always been (cpu->memory[0x8000-0x9FFF]); this is bank 1 only,
    // mirroring cpu.h's wram_bank comment on why a separate array (not
    // swap-into-flat-window) is the pattern this project already uses
    // for banked storage (cart.c's RAM banking). vbk is the raw
    // register value (bit 0 only); both are no-ops in DMG mode.
    uint8_t vram_bank1[0x2000];
    uint8_t vbk;

    // CGB color palette RAM (BCPS/BCPD 0xFF68-69, OCPS/OCPD 0xFF6A-6B) -
    // pandocs' Palettes.md "LCD Color Palettes (CGB only)": 8 palettes x
    // 4 colors x 2 bytes (RGB555, little-endian) = 64 bytes each, for
    // BG/window and OBJ separately. bcps/ocps hold the raw index
    // register (bit 7 auto-increment, bits 5-0 address).
    uint8_t bg_pram[64];
    uint8_t obj_pram[64];
    uint8_t bcps, ocps;

    int frame_ready; // set once per VBlank entry; a driver clears it after grabbing a frame
} GBPpu;

void gb_ppu_reset(GBPpu *ppu);

// Advances the PPU by `cycles` T-states (the same unit gb_cpu_step()
// returns) - called once per gb_cpu_step(), the same way real hardware
// ticks the PPU off the identical clock the CPU runs from. `cpu` is
// needed read-only for VRAM/OAM/palette memory access while rendering,
// and to set IF (0xFF0F) interrupt-request bits on VBlank/STAT events -
// full interrupt *dispatch* is still Phase 4, but there's no reason for
// the PPU side of "an interrupt became pending" to wait for that.
void gb_ppu_step(GBPpu *ppu, struct GBCpu *cpu, int cycles);

// Whether the CPU's own bus access to OAM ($FE00-$FE9F) is currently
// blocked by the PPU itself - pandocs' Rendering.md "PPU modes" table:
// OAM is accessible only during Modes 0/1, not 2 (OAM scan - the PPU is
// using that same bus) or 3 (drawing). Separate from, and in addition
// to, mmu.c's own OAM DMA bus-conflict check - either one blocks access
// on its own. `is_write` selects visible_oam_read_blocked vs.
// visible_oam_write_blocked (see ppu.h's own comment on those fields,
// and gb_ppu_step()'s) - reads and writes genuinely see different
// timing right at the Mode 2->3 boundary, not `mode`/visible_mode
// directly for either.
int gb_ppu_oam_blocked(const GBPpu *ppu, int is_write);

// Whether the CPU's own bus access to VRAM ($8000-$9FFF) is currently
// blocked - pandocs' Rendering.md "PPU modes" table: VRAM is
// inaccessible only during Mode 3 (Drawing), unlike OAM which is also
// blocked during Mode 2. A genuinely separate, previously entirely
// unimplemented gap - found via Mooneye's own lcdon_timing-GS.gb
// (test_roms/mooneye/), whose VRAM-access table needed this to pass at
// all. `is_write` selects visible_vram_read_blocked vs.
// visible_vram_write_blocked, the same read/write asymmetry
// gb_ppu_oam_blocked() has.
int gb_ppu_vram_blocked(const GBPpu *ppu, int is_write);

// `cpu` is needed only to gate the CGB-only registers (VBK, BCPS/BCPD,
// OCPS/OCPD) on cpu->is_cgb - see mmu.c's call site, which routes them
// here alongside the DMG registers rather than through the separate
// inert-stub carve-out mmu.c uses for SVBK (ppu.c already owns this
// whole dispatch, so there's no reason to split CGB PPU registers out
// into mmu.c itself the way the CPU/WRAM-side SVBK register is).
uint8_t gb_ppu_read_reg(GBPpu *ppu, struct GBCpu *cpu, uint16_t addr);
// `cpu` is also needed for the DMA register (0xFF46), which triggers an
// immediate OAM transfer (see gb_ppu_write_reg's own comment on why
// this is instant rather than timed).
void gb_ppu_write_reg(GBPpu *ppu, struct GBCpu *cpu, uint16_t addr, uint8_t val);

#endif
