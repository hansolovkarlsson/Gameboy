#include "ppu.h"
#include "cpu.h"
#include <string.h>

// Real DMG post-boot register values (pandocs' Power_Up_Sequence.md,
// same page Phase 1/2 already cited for CPU registers and the header
// checksum). mode/ly/dots are reset to the deterministic start of a
// fresh frame rather than whatever mid-boot-animation snapshot real
// hardware would show at PC=0x0100 - no test ROM this phase depends on
// that transient state (they all set up LCDC etc. themselves before
// enabling the display). OBP0/OBP1 are genuinely undocumented at
// power-on per pandocs (marked "??" there) - 0xFF is an arbitrary but
// harmless placeholder, not a cited value.
void gb_ppu_reset(GBPpu *ppu) {
    memset(ppu, 0, sizeof(*ppu));
    ppu->lcdc = 0x91;
    ppu->stat = 0x85;
    ppu->bgp = 0xFC;
    ppu->obp0 = 0xFF;
    ppu->obp1 = 0xFF;
    ppu->mode = 2;
    ppu->visible_mode = 2;
    ppu->visible_lyc_flag = (uint8_t)(ppu->stat & 0x04);
    ppu->visible_oam_read_blocked = 1;
    ppu->visible_oam_write_blocked = 1;
    ppu->visible_vram_read_blocked = 0; // Mode 2 doesn't block VRAM, only OAM
    ppu->visible_vram_write_blocked = 0;
    ppu->mode3_dots = 172; // sane default (the real minimum) until the
                            // first Mode 2->3 transition computes a real
                            // one - see compute_mode3_length()
}

static void request_stat_interrupt(struct GBCpu *cpu) {
    gb_write_byte(cpu, 0xFF0F, (uint8_t)(gb_read_byte(cpu, 0xFF0F) | 0x02));
}

// Just the comparison flag itself (stat bit 2) - no interrupt side
// effect here anymore (see update_stat_line() below for why that moved
// out). Callers that change LY or LYC call this, then update_stat_line()
// afterward to let the interrupt line's own edge-detection see the result.
static void update_lyc_flag(GBPpu *ppu) {
    if (ppu->ly == ppu->lyc) ppu->stat |= 0x04; else ppu->stat &= (uint8_t)~0x04;
}

// The STAT interrupt is a level-triggered OR of up to 4 independently-
// enabled conditions (Mode 0/1/2 and LYC==LY) - pandocs'
// Interrupt_Sources.md "INT $48 - STAT interrupt": "the various STAT
// interrupt sources...have their state...logically ORed into a shared
// STAT interrupt line", and "a STAT interrupt will be triggered by a
// rising edge (transition from low to high) on the STAT interrupt
// line" - not by any source's condition merely being true. That same
// page's "STAT blocking" warning is the direct consequence: if a
// source ORs the line high while another source already holds it high,
// there's no edge, so no interrupt.
//
// Modeled here as ppu->stat_line, an explicit level persisted across
// calls (unlike the mode/LYC state it's derived from, which callers
// already track individually) - every call site that can change any of
// the 4 conditions (a mode transition, the LYC comparison flag via
// update_lyc_flag() above, or the STAT register's own select bits
// being written) calls this afterward to recompute the OR and fire on
// a genuine rising edge only.
//
// Confirmed against Mooneye's own stat_irq_blocking.gb
// (test_roms/mooneye/): enabling a select bit for a condition that's
// already true fires an immediate edge (its round 1: enabling Mode 1
// select while already in VBlank), but a LY==LYC coincidence held
// continuously through a Mode 3->0 transition suppresses Mode 0's own
// interrupt entirely, since the line never dropped low in between
// (its round 2).
static void update_stat_line(GBPpu *ppu, struct GBCpu *cpu) {
    int active =
        ((ppu->mode == 0) && (ppu->stat & 0x08)) ||
        ((ppu->mode == 1) && (ppu->stat & 0x10)) ||
        ((ppu->mode == 2) && (ppu->stat & 0x20)) ||
        ((ppu->stat & 0x04) && (ppu->stat & 0x40));
    if (active && !ppu->stat_line) request_stat_interrupt(cpu);
    ppu->stat_line = (uint8_t)active;
}

// The PPU's own internal VRAM access (tile-data/tile-map reads while
// rendering) - reads cpu->memory[]/ppu->vram_bank1[] directly, bypassing
// gb_ppu_vram_blocked() (mmu.c/ppu.h) the same way read_oam_internal()
// bypasses gb_ppu_oam_blocked(): that check exists to stop the *CPU*
// from reaching VRAM during Mode 3, not the PPU itself, which
// obviously still needs to read VRAM throughout Mode 3 to render at
// all. `bank` (0 or 1) is only ever nonzero in CGB mode - every DMG
// call site passes 0, landing on the exact same cpu->memory[] access
// this had before VRAM banking existed.
static uint8_t read_vram_internal(struct GBCpu *cpu, int bank, uint16_t addr) {
    if (bank == 1) return cpu->ppu->vram_bank1[addr - 0x8000];
    return cpu->memory[addr];
}

// Reads one BG/window tile's pixel row and returns the raw 2-bit color
// index (0-3) at column `px` (0-7). Shared by the BG and window paths
// below since both read tiles identically once the tile map base and
// pixel coordinates are worked out - see docs/GAMEBOY_ROADMAP.md's
// Tile_Data.md citation for the addressing modes and bit-packing this
// implements. `bank` selects VRAM bank 0 or 1 for the tile *data* fetch
// (CGB only - pandocs' Tile_Maps.md "BG Map Attributes" bank bit;
// always 0 in DMG mode). BG/window X/Y flip (CGB attribute bits, never
// present in DMG) is applied by the caller pre-flipping px/py before
// calling this, not handled in here - the addressing math below doesn't
// need to know flip happened, only which row/column to fetch.
static uint8_t read_tile_pixel(struct GBCpu *cpu, uint8_t lcdc, uint8_t tile_id, int bank, int px, int py) {
    uint16_t tile_addr;
    if (lcdc & 0x10) {
        tile_addr = (uint16_t)(0x8000 + tile_id * 16); // "$8000 method": unsigned
    } else {
        tile_addr = (uint16_t)(0x9000 + (int8_t)tile_id * 16); // "$8800 method": signed, base $9000
    }
    uint8_t lo_byte = read_vram_internal(cpu, bank, (uint16_t)(tile_addr + py * 2));
    uint8_t hi_byte = read_vram_internal(cpu, bank, (uint16_t)(tile_addr + py * 2 + 1));
    int bit = 7 - px;
    uint8_t lo = (lo_byte >> bit) & 1;
    uint8_t hi = (hi_byte >> bit) & 1;
    return (uint8_t)((hi << 1) | lo);
}

static uint8_t apply_palette(uint8_t palette, uint8_t color_idx) {
    return (uint8_t)((palette >> (color_idx * 2)) & 0x03);
}

// CGB color palette RAM lookup (BCPS/BCPD, OCPS/OCPD - pandocs'
// Palettes.md): `pram` is bg_pram or obj_pram, `palette_num` 0-7,
// `color_idx` 0-3 (the same raw tile color index apply_palette() maps
// through a DMG palette register instead). Colors are stored two bytes
// per color, little-endian, 4 colors per palette - "2 bytes/color x 4
// colors/palette x 8 palettes = 64 bytes" (Palettes.md's own footnote).
static uint16_t apply_cgb_palette(const uint8_t *pram, int palette_num, uint8_t color_idx) {
    int offset = palette_num * 8 + color_idx * 2;
    return (uint16_t)(pram[offset] | (pram[offset + 1] << 8));
}

// Window visibility per pandocs' Tile_Maps.md: WY <= LY for the line,
// and (checked per-pixel by callers) WX-7 <= x. WX <= 166 keeps a
// window that's scrolled fully off the right edge from advancing the
// internal line counter, or incurring compute_mode3_length()'s window
// penalty, for a line nothing was actually drawn on. Shared by
// render_scanline()'s drawing pass and compute_mode3_length()'s window
// timing penalty so both agree on exactly when the window is active.
// `cpu` only to check is_cgb - pandocs' Tile_Maps.md's Window section:
// "in Non-CGB mode [LCDC bit 5] is only functional as long as [LCDC bit
// 0] is set" - implying CGB mode's window-enable bit works independently
// of bit 0 (which in CGB mode means something else entirely - BG/OBJ
// master priority, not BG/window display - see LCDC.md's own CGB-mode
// section for LCDC.0).
static int window_visible_on_line(struct GBCpu *cpu, GBPpu *ppu, int ly) {
    int window_enabled;
    if (cpu->is_cgb) {
        window_enabled = (ppu->lcdc & 0x20) != 0;
    } else {
        int bg_win_enabled = (ppu->lcdc & 0x01) != 0;
        window_enabled = bg_win_enabled && (ppu->lcdc & 0x20) != 0;
    }
    return window_enabled && (ppu->wy <= ly) && (ppu->wx <= 166);
}

// Selects up to 10 objects overlapping scanline `ly` (pandocs' OAM.md
// selection priority: OAM scan order, first 10 kept) and sorts them
// into real drawing/priority order (smaller X first, ties broken by
// OAM order - a stable insertion sort suffices since the scan above
// already yields ascending OAM order). Shared by render_scanline()'s
// drawing pass and compute_mode3_length()'s per-object timing penalty:
// pandocs' Rendering.md footnote on OBJ penalty ordering explicitly
// matches this same leftmost-first/OAM-tiebreak order, so both need to
// agree on it exactly, not just happen to produce similar results.
// Reads OAM directly from cpu->memory[], not through gb_read_byte() -
// this is the PPU's own internal access (object selection, Mode 3
// length computation, rendering), which real hardware's OAM DMA and
// Mode 2/3 CPU-bus-conflict logic never blocks (that logic exists
// specifically to stop the *CPU* from also reaching OAM while the PPU
// itself is using that bus - see gb_ppu_oam_blocked(), ppu.h/mmu.c).
static uint8_t read_oam_internal(struct GBCpu *cpu, uint16_t addr) {
    return cpu->memory[addr];
}

// `sort_by_x`: DMG drawing/timing priority is smaller-X-first (pandocs'
// OAM.md "Drawing priority", Non-CGB Mode) - the insertion sort below.
// CGB drawing priority is OAM-order-only (same page, CGB Mode: "only the
// object's location in OAM determines its priority"), which the initial
// scan loop above already produces on its own, so the sort is simply
// skipped - not a different algorithm, just not applying the DMG-only
// tiebreak-by-X step at all. compute_mode3_length()'s own call always
// passes 1 (DMG's real, ROM-verified timing order) regardless of mode -
// no CGB test ROM has verified this project's Mode 3 timing under CGB
// yet, so that call is deliberately left exactly as before rather than
// guessed at; only render_scanline()'s drawing-priority call varies by
// mode, since real CGB *drawing* order is documented and unambiguous.
static int select_objects_for_scanline(struct GBCpu *cpu, int ly, int obj_height, int selected[10], int sort_by_x) {
    int selected_count = 0;
    for (int i = 0; i < 40 && selected_count < 10; i++) {
        uint16_t oam_addr = (uint16_t)(0xFE00 + i * 4);
        uint8_t obj_y = read_oam_internal(cpu, oam_addr);
        int obj_top = obj_y - 16;
        if (ly >= obj_top && ly < obj_top + obj_height) {
            selected[selected_count++] = i;
        }
    }

    if (!sort_by_x) return selected_count;
    for (int a = 1; a < selected_count; a++) {
        int key = selected[a];
        uint8_t key_x = read_oam_internal(cpu, (uint16_t)(0xFE00 + key * 4 + 1));
        int b = a - 1;
        while (b >= 0) {
            uint8_t b_x = read_oam_internal(cpu, (uint16_t)(0xFE00 + selected[b] * 4 + 1));
            if (b_x <= key_x) break;
            selected[b + 1] = selected[b];
            b--;
        }
        selected[b + 1] = key;
    }
    return selected_count;
}

// pandocs' Rendering.md "Mode 3 length" - real hardware's exact,
// confirmed algorithm (distinct from pixel_fifo.md's own hedged "OBJ
// penalty timing... not confirmed" wording for a *different*, more
// speculative interaction - this function follows Rendering.md, the
// more authoritative of the two on this specific question). Computed
// once at the Mode 2->3 transition, from a snapshot of LCDC/SCX/WY/WX/
// OAM at that instant - see ppu.h's own comment for why this is
// "exact duration, not full per-dot FIFO simulation".
static int compute_mode3_length(GBPpu *ppu, struct GBCpu *cpu) {
    int ly = ppu->ly;
    int dots = 172; // 160 output dots + 12 (two tile-fetch startup, pandocs' own footnote)
    dots += ppu->scx & 7; // scroll penalty: SCX%8 dots stalled discarding that many leftmost pixels

    int window_visible_this_line = window_visible_on_line(cpu, ppu, ly);
    if (window_visible_this_line) dots += 6; // flat penalty: fetcher reset/setup for the window

    ppu->mode3_had_obj = 0;
    if (!(ppu->lcdc & 0x02)) return dots; // objects disabled - no OBJ penalties at all

    int obj_height = (ppu->lcdc & 0x04) ? 16 : 8;
    int selected[10];
    int selected_count = select_objects_for_scanline(cpu, ly, obj_height, selected, 1);
    ppu->mode3_had_obj = selected_count > 0; // see gb_ppu_step()'s own comment on why this
                                              // matters for how Mode 3->0 becomes observable

    // "Tiles already considered by a previous OBJ" - at most
    // selected_count entries are ever needed (one per object,
    // including OAM-X==0 ones - see the loop's own comment on why
    // those still participate in this dedup).
    int considered_is_window[10];
    int considered_col[10];
    int considered_count = 0;

    for (int s = 0; s < selected_count; s++) {
        uint16_t oam_addr = (uint16_t)(0xFE00 + selected[s] * 4);
        uint8_t obj_x = read_oam_internal(cpu, (uint16_t)(oam_addr + 1));

        // "The Pixel": the OBJ's own leftmost screen column. obj_x is
        // 0-255, so this can be as low as -8 (obj_x==0, "The Pixel"
        // exception below) or as low as -7 for an object mostly
        // off-screen to the left - still a real, valid column for
        // tile-lookup purposes below.
        int pixel = (int)obj_x - 8;

        // An OBJ entirely off the right edge of the screen (its
        // leftmost column already at or past column 160) is never
        // reached by the pixel fetcher during this scanline's Mode 3
        // at all - not even its flat 6-dot fetch cost. Found via
        // Mooneye's own intr_2_mode0_timing_sprites.gb
        // (test_roms/mooneye/): its obj_x=168/169 testcases (pixel
        // 160/161) are the only ones in the suite asserting zero OBJ
        // penalty despite objects being selected for the line.
        if (pixel >= GB_SCREEN_WIDTH) continue;

        int use_window = window_visible_this_line && (pixel + 7 >= ppu->wx);
        int col, pixel_in_tile;
        if (use_window) {
            // Only reached when pixel+7 >= wx, i.e. wx_pixel >= 0 -
            // never negative, so plain / and % are safe here.
            int wx_pixel = pixel - (ppu->wx - 7);
            col = wx_pixel / 8;
            pixel_in_tile = wx_pixel % 8;
        } else {
            // +256 before masking: pixel can be mildly negative (down
            // to -8) and scx is 0-255, so this keeps the sum
            // non-negative going into & 0xFF rather than relying on
            // two's-complement wraparound of a negative value.
            int bg_x = (pixel + ppu->scx + 256) & 0xFF;
            col = bg_x / 8;
            pixel_in_tile = bg_x % 8;
        }

        int already_considered = 0;
        for (int t = 0; t < considered_count; t++) {
            if (considered_is_window[t] == use_window && considered_col[t] == col) {
                already_considered = 1;
                break;
            }
        }
        if (!already_considered) {
            // Exception: an OBJ with OAM X position 0 always incurs a
            // flat 5-dot wait component here (instead of the normal
            // "pixels right of The Pixel, minus 2" count) - combined
            // with the unconditional +6 below, this is pandocs'
            // documented "always incurs an 11-dot penalty" for an
            // isolated OBJ at X==0. But unlike the previous
            // implementation (which skipped this whole tile-dedup
            // mechanism for X==0 via an early continue), this OBJ
            // still marks its tile as considered - a second OBJ
            // landing on the same off-screen-left tile (X==0 or not)
            // only pays the flat +6 below, not another 5-dot wait.
            // Found via Mooneye's own intr_2_mode0_timing_sprites.gb:
            // its multi-OBJ-at-X==0 testcases (2 through 10 OBJs, all
            // X==0) assert exactly 11 + 6*(n-1) dots, not 11*n.
            int wait = (obj_x == 0) ? 5 : (7 - pixel_in_tile - 2 /* pixels_right - 2 */);
            if (wait > 0) dots += wait;
            if (considered_count < 10) {
                considered_is_window[considered_count] = use_window;
                considered_col[considered_count] = col;
                considered_count++;
            }
        }

        dots += 6; // flat OBJ tile-fetch cost - incurred by every object, tile-sharing or not
    }

    return dots;
}

static void render_scanline(GBPpu *ppu, struct GBCpu *cpu) {
    int ly = ppu->ly;
    int is_cgb = cpu->is_cgb;
    uint8_t bg_color_idx[GB_SCREEN_WIDTH];
    // BG map attribute bit 7 ("BG-to-OBJ priority") per pixel - CGB
    // only, needed by the object loop below for the real 3-way priority
    // rule (pandocs' Tile_Maps.md "BG-to-OBJ Priority in CGB Mode").
    // Left all-zero in DMG mode, where it's simply unused.
    uint8_t bg_cgb_priority[GB_SCREEN_WIDTH] = {0};

    // DMG: bit 0 clear blanks BG/window entirely (white). CGB: bit 0
    // clear instead means "objects always win" - BG/window still
    // render normally - handled by the object-compositing loop below,
    // not here. See LCDC.md's own CGB-mode section for LCDC.0.
    int bg_win_enabled = is_cgb || (ppu->lcdc & 0x01) != 0;
    int window_visible_this_line = window_visible_on_line(cpu, ppu, ly);
    int window_drawn_this_line = 0;

    for (int x = 0; x < GB_SCREEN_WIDTH; x++) {
        if (!bg_win_enabled) {
            // pandocs' LCDC.md: "both background and window become
            // blank (white)" - literal white, not "whatever BGP maps
            // color 0 to". DMG-only path (is_cgb already forced
            // bg_win_enabled true above).
            bg_color_idx[x] = 0;
            ppu->framebuffer[ly][x] = 0;
            continue;
        }

        int use_window = window_visible_this_line && (x + 7 >= ppu->wx);
        uint16_t tile_map_base;
        int tile_col, tile_row, px, py;
        if (use_window) {
            window_drawn_this_line = 1;
            tile_map_base = (ppu->lcdc & 0x40) ? 0x9C00 : 0x9800;
            int wx_pixel = x - (ppu->wx - 7);
            tile_col = wx_pixel / 8;
            px = wx_pixel % 8;
            tile_row = ppu->window_line / 8;
            py = ppu->window_line % 8;
        } else {
            tile_map_base = (ppu->lcdc & 0x08) ? 0x9C00 : 0x9800;
            int bg_x = (x + ppu->scx) & 0xFF;
            int bg_y = (ly + ppu->scy) & 0xFF;
            tile_col = bg_x / 8;
            px = bg_x % 8;
            tile_row = bg_y / 8;
            py = bg_y % 8;
        }

        uint16_t map_addr = (uint16_t)(tile_map_base + tile_row * 32 + tile_col);
        uint8_t tile_id = read_vram_internal(cpu, 0, map_addr);

        int tile_bank = 0, palette_num = 0;
        if (is_cgb) {
            // CGB BG map attribute byte: VRAM bank 1, same address as
            // the tile ID in bank 0 (pandocs' Tile_Maps.md).
            uint8_t attr = read_vram_internal(cpu, 1, map_addr);
            palette_num = attr & 0x07;
            tile_bank = (attr & 0x08) ? 1 : 0;
            if (attr & 0x20) px = 7 - px; // X flip
            if (attr & 0x40) py = 7 - py; // Y flip
            bg_cgb_priority[x] = (attr & 0x80) ? 1 : 0;
        }
        uint8_t color_idx = read_tile_pixel(cpu, ppu->lcdc, tile_id, tile_bank, px, py);

        bg_color_idx[x] = color_idx;
        if (is_cgb) {
            ppu->cgb_framebuffer[ly][x] = apply_cgb_palette(ppu->bg_pram, palette_num, color_idx);
        } else {
            ppu->framebuffer[ly][x] = apply_palette(ppu->bgp, color_idx);
        }
    }

    if (window_drawn_this_line) ppu->window_line++;

    if (!(ppu->lcdc & 0x02)) return; // objects disabled

    int obj_height = (ppu->lcdc & 0x04) ? 16 : 8;
    int selected[10];
    // DMG: X-sorted drawing priority. CGB: OAM order only - see
    // select_objects_for_scanline()'s own comment.
    int selected_count = select_objects_for_scanline(cpu, ly, obj_height, selected, !is_cgb);

    // claimed[x]: an opaque pixel from a higher-priority object already
    // resolved this column, win or lose against BG - per pandocs'
    // "Interaction with BG over OBJ flag" note, priority between
    // objects is resolved *before* BG priority is considered, so a
    // higher-priority object's opaque-but-BG-losing pixel still blocks
    // a lower-priority object from being drawn there at all.
    uint8_t claimed[GB_SCREEN_WIDTH] = {0};

    for (int s = 0; s < selected_count; s++) {
        int i = selected[s];
        uint16_t oam_addr = (uint16_t)(0xFE00 + i * 4);
        uint8_t obj_y = read_oam_internal(cpu, oam_addr);
        uint8_t obj_x = read_oam_internal(cpu, (uint16_t)(oam_addr + 1));
        uint8_t tile_id = read_oam_internal(cpu, (uint16_t)(oam_addr + 2));
        uint8_t attr = read_oam_internal(cpu, (uint16_t)(oam_addr + 3));

        int obj_top = obj_y - 16;
        int obj_left = obj_x - 8;
        int y_flip = (attr & 0x40) != 0;
        int x_flip = (attr & 0x20) != 0;
        int obj_bg_priority = (attr & 0x80) != 0; // DMG: "OBJ behind BG colors 1-3". CGB: one of 3 priority inputs - see below.
        int obj_tile_bank = is_cgb && (attr & 0x08) ? 1 : 0; // CGB only - pandocs' OAM.md "Bank"
        int cgb_palette_num = attr & 0x07; // CGB only - "CGB palette"
        uint8_t palette = (attr & 0x10) ? ppu->obp1 : ppu->obp0; // DMG only - "DMG palette"

        int row = ly - obj_top;
        if (y_flip) row = obj_height - 1 - row;
        if (obj_height == 16) tile_id &= 0xFE; // "top 8x8 tile is NN & $FE" - pandocs' OAM.md

        // Objects always use $8000 addressing regardless of LCDC.4
        // (pandocs' Tile_Data.md) - read directly rather than going
        // through read_tile_pixel(), which honors LCDC.4 for BG/window.
        uint16_t tile_addr = (uint16_t)(0x8000 + tile_id * 16 + row * 2);
        uint8_t lo_byte = read_vram_internal(cpu, obj_tile_bank, tile_addr);
        uint8_t hi_byte = read_vram_internal(cpu, obj_tile_bank, (uint16_t)(tile_addr + 1));

        for (int col = 0; col < 8; col++) {
            int x = obj_left + col;
            if (x < 0 || x >= GB_SCREEN_WIDTH || claimed[x]) continue;

            int bit = x_flip ? col : (7 - col);
            uint8_t lo = (lo_byte >> bit) & 1;
            uint8_t hi = (hi_byte >> bit) & 1;
            uint8_t color_idx = (uint8_t)((hi << 1) | lo);
            if (color_idx == 0) continue; // transparent for objects

            claimed[x] = 1;

            if (is_cgb) {
                // pandocs' Tile_Maps.md "BG-to-OBJ Priority in CGB
                // Mode": BG color 0 always loses to OBJ; else LCDC.0
                // clear always gives OBJ priority (master toggle, see
                // bg_win_enabled's own comment above); else OBJ wins
                // only if *both* the BG attribute and OAM attribute
                // priority bits are clear - otherwise BG (colors 1-3)
                // wins.
                if (bg_color_idx[x] != 0 && (ppu->lcdc & 0x01) &&
                    (bg_cgb_priority[x] || obj_bg_priority)) {
                    continue;
                }
                ppu->cgb_framebuffer[ly][x] = apply_cgb_palette(ppu->obj_pram, cgb_palette_num, color_idx);
            } else {
                if (obj_bg_priority && bg_color_idx[x] != 0) continue; // BG colors 1-3 win
                ppu->framebuffer[ly][x] = apply_palette(palette, color_idx);
            }
        }
    }
}

void gb_ppu_step(GBPpu *ppu, struct GBCpu *cpu, int cycles) {
    if (!(ppu->lcdc & 0x80)) return; // LCD off: PPU fully idle, per pandocs' LCDC.md

    // Snapshot the mode STAT reads *before* this call's own transition
    // check below can change it - gb_mcycle_tick() (mmu.c) runs this,
    // then immediately performs the CPU's own memory access for that
    // same M-cycle, so a register read landing on the *exact* M-cycle a
    // mode transition occurs would otherwise already observe the *new*
    // mode. Real hardware doesn't make a transition externally visible
    // that fast - only from the *next* M-cycle on - while the internal
    // `mode` interrupts key off of (via update_stat_line() and the
    // VBlank-quirk check below) *does* need to change immediately, at
    // the real transition instant, since those are independently
    // already verified correct (stat_irq_blocking.gb, vblank_stat_intr-
    // GS.gb, stat_lyc_onoff.gb). Found via Mooneye's own
    // acceptance/ppu/intr_2_mode0_timing.gb (test_roms/mooneye/): its
    // two NOP-padded polling loops (46 vs 45 NOPs) are built to land
    // exactly on vs. one M-cycle before a Mode 3->0 boundary; without
    // this lag both resolved to the same iteration count instead of the
    // ROM's own asserted one-iteration difference.
    ppu->visible_mode = ppu->mode;
    // The LY==LYC comparison flag (STAT bit 2) gets the same one-M-cycle
    // read lag as the mode bits (see ppu.h's own comment on
    // visible_lyc_flag) - but with one more real wrinkle on top: a
    // genuine comparator glitch, not just a read-visibility lag. On the
    // exact M-cycle LY is about to increment, the flag reads as clear
    // *regardless of whether the new LY will match LYC* - not "the old
    // pre-increment comparison" (which a plain snapshot-before-mutation
    // lag, like visible_mode's, would give), and not yet "the new post-
    // increment comparison" either. It only settles into the real,
    // correct comparison starting the M-cycle *after* that. Very
    // plausibly a real ripple-counter artifact (LY's low bits are
    // briefly in an invalid transitional state to any comparator
    // watching them combinationally) rather than anything deliberately
    // designed, but Mooneye's own lcdon_timing-GS.gb (test_roms/
    // mooneye/) - whose STAT-with-two-different-LYC-values tables both
    // assert flag-clear at the exact M-cycle LY increments from 0 to 1,
    // regardless of LYC being 0 (where the naive lagged comparison
    // would say "match, flag set") or 1 (where the naive *unlagged*
    // comparison would also say "match, flag set") - leaves no
    // reading of the mechanism that isn't a genuine forced-clear.
    // Applied to every LY increment in this function (Mode 0->1/2 and
    // VBlank's own line-by-line increment, including the 153->0
    // wraparound), not just this one line0-after-LCD-enable case that
    // happened to be where it was found - no data yet says it's
    // narrower than "every LY increment", and Mode 0/1's transition
    // conditions checked here mirror the real ones lower down exactly.
    int ly_about_to_change =
        (!ppu->lcd_starting && ppu->mode == 0 && ppu->dots + cycles >= 376 - ppu->mode3_dots) ||
        (ppu->mode == 1 && ppu->dots + cycles >= 456);
    ppu->visible_lyc_flag = ly_about_to_change ? 0 : (uint8_t)(ppu->stat & 0x04);
    // Bus arbitration (OAM/VRAM access) has two independent real
    // quirks, genuinely different for CPU reads vs. writes *and* for
    // OAM vs. VRAM - not the same signal reused four ways. Neither is
    // shared with visible_mode (STAT's own mode bits read correctly
    // the whole way through both, confirmed against the same ROMs' own
    // STAT table sampled at the identical M-cycles):
    //
    // OAM reads see the same LY-increment glitch as visible_lyc_flag
    // above (blocked on the exact M-cycle LY is about to increment,
    // even though the mode bits themselves still correctly read 0
    // right then) and are otherwise unaffected by the Mode 2->3
    // boundary below.
    //
    // OAM writes instead see an early one-M-cycle *unblock* right
    // before Mode 3 becomes STAT-visible: OAM scan has already
    // finished with the bus by then, freeing it for a CPU write a
    // M-cycle before the mode value and STAT bits themselves switch
    // over - even though a CPU *read* at that same M-cycle is still
    // contested as if OAM scan's own reads were still using the bus
    // (plain Mode 2 behavior, unaffected).
    //
    // VRAM has the mirror image: reads see an early one-M-cycle
    // *block* at that same boundary (the Mode 3 pixel fetcher has
    // already begun claiming the bus to prefetch, contesting a CPU
    // read there a M-cycle before Mode 3 itself is STAT-visible),
    // while VRAM *writes* are unaffected and follow the plain Mode 3
    // rule with no early transition at all.
    //
    // Found via Mooneye's own lcdon_timing-GS.gb (reads) disagreeing
    // with lcdon_write_timing-GS.gb (writes) (test_roms/mooneye/) at
    // these exact same M-cycles - not coincidentally-adjacent bugs,
    // but a real, mirrored read/write/OAM/VRAM split, all four
    // combinations independently confirmed against both ROMs' own
    // data. Strict > (not >=) in mode2_bus_handoff: dots stays >= this
    // threshold for two consecutive M-cycles once reached (it only
    // resets when the real transition actually fires, one M-cycle
    // later than first crossing it), but the real handoff is exactly
    // one M-cycle wide - >= would wrongly widen it to two.
    int mode2_bus_handoff = (ppu->mode == 2 && ppu->dots + cycles > 76);
    ppu->visible_oam_read_blocked = ly_about_to_change || (ppu->mode == 2 || ppu->mode == 3);
    ppu->visible_oam_write_blocked = (ppu->mode == 2 || ppu->mode == 3) && !mode2_bus_handoff;
    ppu->visible_vram_read_blocked = (ppu->mode == 3) || mode2_bus_handoff;
    ppu->visible_vram_write_blocked = (ppu->mode == 3);

    ppu->dots += cycles;

    switch (ppu->mode) {
        case 2: // OAM scan
            if (ppu->dots >= 80) {
                ppu->dots -= 80;
                ppu->mode = 3;
                // Computed once here, from the register/OAM snapshot as
                // of the Mode 2->3 transition - see compute_mode3_length()
                // and ppu.h's own comment on why this is exact duration,
                // not a full per-dot simulation.
                ppu->mode3_dots = compute_mode3_length(ppu, cpu);
            }
            break;

        case 3: { // Drawing - pandocs' Rendering.md "Mode 3 length" algorithm (see compute_mode3_length())
            // Whenever this scanline actually fetched >=1 OBJ, the
            // transition becomes observable up to 3 dots *earlier*
            // than mode3_dots's own exact value - i.e. against a
            // threshold rounded *down* to the nearest whole M-cycle
            // (dots only ever accumulates in whole 4-dot steps, so
            // this is the largest multiple of 4 not exceeding
            // mode3_dots). ppu->dots itself still accumulates and
            // carries into Mode 0 using the *unrounded* mode3_dots
            // (below), so the scanline's total dot budget (456) is
            // unaffected - Mode 0 simply absorbs however many dots
            // Mode 3 "gave back", the same way it already absorbs
            // mode3_dots's own fractional-of-4 remainder on an OBJ-
            // free scanline. An OBJ-free scanline (still possibly
            // landing on an exact multiple of 4 via SCX%8 + the
            // window's flat 6, e.g. SCX%8==0) genuinely does NOT get
            // this rounding - confirmed by every already-passing non-
            // sprite acceptance/ppu/ ROM regressing when this was
            // first tried as an unconditional early transition (both
            // as a flat -1 M-cycle and as a naive "> instead of >="
            // strict-boundary rule - the latter is actually the wrong
            // *direction*: it makes an exact-multiple mode3_dots take
            // one M-cycle *longer*, not shorter, and independently-
            // computing Mooneye's own required M-cycle counts by hand
            // against its intr_2_mode0_timing_sprites.gb source showed
            // the real requirement is consistently 1 M-cycle *earlier*
            // than plain ceil(mode3_dots/4), not later). Found via
            // Mooneye's own intr_2_mode0_timing_sprites.gb (test_roms/
            // mooneye/): its 105 OBJ-count/position testcases, hand-
            // decoded from the ROM's own .s source and cross-checked
            // against this exact rounding rule, match it in every
            // single case; its obj_x=168/169 testcases (every OBJ
            // selected for the line but skipped as off-screen-right,
            // landing back on the already-multiple-of-4 flat 172)
            // still needed the OBJ-triggered rounding path despite it
            // being a no-op there, confirming this keys off of "was an
            // OBJ fetched for this scanline", not "is mode3_dots
            // non-round".
            int mode3_boundary = ppu->mode3_had_obj ? (ppu->mode3_dots & ~3) : ppu->mode3_dots;
            if (ppu->dots >= mode3_boundary) {
                ppu->dots -= ppu->mode3_dots;
                render_scanline(ppu, cpu);
                ppu->mode = 0;
                update_stat_line(ppu, cpu); // Mode 0 int select, via the shared line - see its own comment
            }
            break;
        }

        case 0: // HBlank: the rest of the scanline - pandocs' own table:
                // Mode 0's duration is 376 - Mode 3's (80 + 376 = 456 total,
                // matching Mode 2's fixed 80 dots above).
            if (ppu->lcd_starting) {
                // Real hardware quirk, distinct from every other Mode 0:
                // line 0 immediately after LCD-enable never has a real
                // Mode 2 (OAM scan) at all. STAT reports mode 0 from the
                // moment the LCD-enable write lands, for a short, fixed
                // 76-dot window, then goes *directly* to Mode 3 - no LY
                // increment, no Mode 2 STAT interrupt, nothing else
                // Mode 2 would normally do. Everything from here on
                // (this Mode 3's own length via compute_mode3_length(),
                // the real Mode 0 that follows it, and line 1 onward) is
                // completely ordinary - confirmed by reverse-engineering
                // Mooneye's own lcdon_timing-GS.gb (test_roms/mooneye/):
                // its LY/STAT-with-two-LYC-settings/OAM-access/VRAM-
                // access tables, sampled at M-cycle-precise offsets from
                // the LCDC write across 3 NOP-shifted passes, show
                // mode 0->3 for line 0 bracketed to a single M-cycle
                // (consistent with any threshold in 73-76 dots - this
                // ROM's own M-cycle-granularity sampling can't
                // distinguish within that range, so 76 was chosen as the
                // cleanest boundary), and mode 3's and the real Mode 0's
                // own durations for line 0 exactly matching their normal
                // 172/204-dot values once this initial window is over -
                // not a shortened variant of either. 76 dots is 4 dots
                // (1 M-cycle) short of Mode 2's normal 80 - this project
                // found no data pinning down *why* by exactly that
                // amount (real hardware's own explanation is undocumented
                // on pandocs), only that the ROM's data requires it.
                if (ppu->dots >= 76) {
                    ppu->dots -= 76;
                    ppu->lcd_starting = 0;
                    ppu->mode = 3;
                    // No update_stat_line() call here, matching the
                    // normal Mode 2->3 transition just above: Mode 3 has
                    // no STAT interrupt select bit of its own to fire.
                    ppu->mode3_dots = compute_mode3_length(ppu, cpu);
                }
                break;
            }
            if (ppu->dots >= 376 - ppu->mode3_dots) {
                ppu->dots -= 376 - ppu->mode3_dots;
                ppu->ly++;
                update_lyc_flag(ppu);
                if (ppu->ly == 144) {
                    ppu->mode = 1;
                    gb_write_byte(cpu, 0xFF0F, (uint8_t)(gb_read_byte(cpu, 0xFF0F) | 0x01)); // VBlank
                    // Real hardware quirk (Mooneye's own acceptance/ppu/
                    // vblank_stat_intr-GS.gb, test_roms/mooneye/, and
                    // Gekkio's mooneye-gb reference emulator,
                    // hardware/ppu.rs's switch_mode() VBlank arm): the
                    // Mode 2 (OAM) STAT condition also glitches true
                    // right at this exact transition, independently of
                    // the shared line's normal edge-detection (real
                    // mode is about to become 1, not 2) - so this fires
                    // as its own direct, unconditional check alongside
                    // update_stat_line() below rather than through it.
                    // Not documented on pandocs' general STAT.md page;
                    // grounded entirely against this test ROM's own
                    // header comment and mooneye-gb.
                    if (ppu->stat & 0x20) request_stat_interrupt(cpu); // Mode 2 int select (quirk)
                    update_stat_line(ppu, cpu); // Mode 1 int select + LYC, via the shared line
                    ppu->frame_ready = 1;
                } else {
                    ppu->mode = 2;
                    update_stat_line(ppu, cpu); // Mode 2 int select, via the shared line
                }
            }
            break;

        case 1: // VBlank: 10 scanlines of 456 dots each
            if (ppu->dots >= 456) {
                ppu->dots -= 456;
                ppu->ly++;
                if (ppu->ly > 153) {
                    ppu->ly = 0;
                    ppu->window_line = 0; // new frame
                    ppu->mode = 2;
                }
                update_lyc_flag(ppu);
                update_stat_line(ppu, cpu); // Mode 2 int select (on wraparound) + LYC, via the shared line
            }
            break;
    }
}

int gb_ppu_oam_blocked(const GBPpu *ppu, int is_write) {
    return (ppu->lcdc & 0x80) && (is_write ? ppu->visible_oam_write_blocked : ppu->visible_oam_read_blocked);
}

int gb_ppu_vram_blocked(const GBPpu *ppu, int is_write) {
    return (ppu->lcdc & 0x80) && (is_write ? ppu->visible_vram_write_blocked : ppu->visible_vram_read_blocked);
}

// CGB CRAM read (BCPD/OCPD) - pandocs' Palettes.md: "the CRAM data
// registers are inaccessible when the PPU is reading from CRAM, that
// is, during Mode 3: ...reads return $FF." Checked against the plain
// live `mode`, not the visible_*_blocked lag-corrected mechanism
// OAM/VRAM use - no CGB test ROM has yet confirmed whether CRAM shares
// those same sub-M-cycle edge cases, so this only implements the
// textually-documented rule, not a guessed extension of it.
static uint8_t read_cram(const uint8_t *pram, uint8_t cps, int mode) {
    if (mode == 3) return 0xFF;
    return pram[cps & 0x3F];
}

// CGB CRAM write, plus BCPS/OCPS's auto-increment side effect - pandocs'
// Palettes.md: "the address field is automatically incremented...after
// each write to this register, even if the write fails due to CRAM
// being inaccessible" - so the increment happens unconditionally, only
// the actual byte write is skipped during Mode 3.
static void write_cram(uint8_t *pram, uint8_t *cps, uint8_t val, int mode) {
    if (mode != 3) pram[*cps & 0x3F] = val;
    if (*cps & 0x80) *cps = (uint8_t)((*cps & 0xC0) | ((*cps + 1) & 0x3F));
}

uint8_t gb_ppu_read_reg(GBPpu *ppu, struct GBCpu *cpu, uint16_t addr) {
    // CGB-only registers - pandocs' Power_Up_Sequence.md footnote:
    // "only available in CGB Mode, and will read $FF in Non-CGB Mode",
    // same convention mmu.c's SVBK carve-out uses.
    if (!cpu->is_cgb) {
        if (addr == 0xFF4F || (addr >= 0xFF68 && addr <= 0xFF6B)) return 0xFF;
    } else {
        switch (addr) {
            case 0xFF4F: return (uint8_t)(ppu->vbk | 0xFE); // bits 1-7 unused, always read 1
            case 0xFF68: return ppu->bcps;
            case 0xFF69: return read_cram(ppu->bg_pram, ppu->bcps, ppu->mode);
            case 0xFF6A: return ppu->ocps;
            case 0xFF6B: return read_cram(ppu->obj_pram, ppu->ocps, ppu->mode);
            default: break;
        }
    }
    switch (addr) {
        case 0xFF40: return ppu->lcdc;
        case 0xFF41: {
            // Mode bits are live-computed, not stored redundantly;
            // pandocs' STAT.md: "Reports 0 instead when the PPU is
            // disabled." Bit 7 is unused and always reads as 1
            // (confirmed against Mooneye's real-hardware-verified
            // unused_hwio-GS.gb, test_roms/mooneye/) regardless of
            // what was last written to it. Uses visible_mode, not mode
            // directly - see gb_ppu_step()'s own comment on why a
            // same-instant register read needs that one-M-cycle lag.
            // Bit 2 (LY==LYC) gets the identical lag via
            // visible_lyc_flag - but only while the LCD is actually on;
            // gb_ppu_step() (where that snapshot happens) returns
            // immediately without updating it while the LCD is off, so
            // falling back to the live ppu->stat bit here keeps that
            // case exactly as before (Mooneye's own stat_lyc_onoff.gb,
            // test_roms/mooneye/, already covers LCD-off LYC behavior
            // and must not regress).
            uint8_t mode = (ppu->lcdc & 0x80) ? (uint8_t)ppu->visible_mode : 0;
            uint8_t lyc_flag = (ppu->lcdc & 0x80) ? ppu->visible_lyc_flag : (uint8_t)(ppu->stat & 0x04);
            return (uint8_t)((ppu->stat & 0xF8) | lyc_flag | mode | 0x80);
        }
        case 0xFF42: return ppu->scy;
        case 0xFF43: return ppu->scx;
        case 0xFF44: return ppu->ly;
        case 0xFF45: return ppu->lyc;
        case 0xFF46: return ppu->dma;
        case 0xFF47: return ppu->bgp;
        case 0xFF48: return ppu->obp0;
        case 0xFF49: return ppu->obp1;
        case 0xFF4A: return ppu->wy;
        case 0xFF4B: return ppu->wx;
        default: return 0xFF;
    }
}

void gb_ppu_write_reg(GBPpu *ppu, struct GBCpu *cpu, uint16_t addr, uint8_t val) {
    if (!cpu->is_cgb) {
        if (addr == 0xFF4F || (addr >= 0xFF68 && addr <= 0xFF6B)) return; // no-op in DMG mode
    } else {
        switch (addr) {
            case 0xFF4F: ppu->vbk = val & 0x01; return;
            case 0xFF68: ppu->bcps = val; return;
            case 0xFF69: write_cram(ppu->bg_pram, &ppu->bcps, val, ppu->mode); return;
            case 0xFF6A: ppu->ocps = val; return;
            case 0xFF6B: write_cram(ppu->obj_pram, &ppu->ocps, val, ppu->mode); return;
            default: break;
        }
    }
    switch (addr) {
        case 0xFF40: {
            int was_on = (ppu->lcdc & 0x80) != 0;
            int now_on = (val & 0x80) != 0;
            if (was_on && !now_on) {
                // LCD turning off - pandocs' LCDC.md warns this should
                // only happen during VBlank (real hardware can be
                // damaged otherwise); not enforced/hard-failed here,
                // just documented. Reset to a fresh frame's start so
                // re-enabling later behaves predictably. The LY==LYC
                // comparison flag (stat bit 2) is deliberately left
                // untouched here - Mooneye's own stat_lyc_onoff.gb
                // (test_roms/mooneye/) shows real hardware retains
                // whatever it last was while the LCD is off, not reset
                // to 0.
                ppu->mode = 0;
                ppu->visible_mode = 0;
                ppu->visible_oam_read_blocked = 0; // Mode 0/1 never block OAM
                ppu->visible_oam_write_blocked = 0;
                ppu->visible_vram_read_blocked = 0; // ...or VRAM
                ppu->visible_vram_write_blocked = 0;
                ppu->dots = 0;
                ppu->ly = 0;
                ppu->mode3_dots = 172; // same reasoning as gb_ppu_reset()
                ppu->mode3_had_obj = 0;
                ppu->lcd_starting = 0;
            }
            ppu->lcdc = val;
            if (!was_on && now_on) {
                // LCD turning back on restarts the LY==LYC "comparison
                // clock" (pandocs' STAT.md: the flag is "constantly
                // updated") - immediately re-evaluate once against the
                // freshly-reset LY=0, firing a real interrupt if that's
                // a newly-true comparison. Grounded against Mooneye's
                // stat_lyc_onoff.gb, whose round 4 explicitly expects
                // exactly this interrupt.
                update_lyc_flag(ppu);
                update_stat_line(ppu, cpu);
                // See gb_ppu_step()'s Mode-0 case for the real quirk this
                // flag drives: line 0 never has a real Mode 2 at all.
                ppu->lcd_starting = 1;
            }
            break;
        }
        case 0xFF41:
            // Bits 0-2 (mode, LYC==LY) stay PPU-owned regardless of
            // what the CPU writes - only the interrupt-select bits
            // (3-6) and the unused bit 7 are genuinely writable.
            ppu->stat = (uint8_t)((ppu->stat & 0x07) | (val & 0xF8));
            // Newly enabling a select bit for a condition that's
            // already true is itself a rising edge on the shared line
            // (update_stat_line()'s own comment) - Mooneye's
            // stat_irq_blocking.gb (test_roms/mooneye/) round 1 depends
            // on exactly this: enabling Mode 1 select while already in
            // VBlank fires an immediate interrupt.
            if (ppu->lcdc & 0x80) update_stat_line(ppu, cpu);
            break;
        case 0xFF42: ppu->scy = val; break;
        case 0xFF43: ppu->scx = val; break;
        case 0xFF44: break; // read-only
        case 0xFF45:
            ppu->lyc = val;
            // The comparator is "constantly updated" (pandocs' STAT.md)
            // only while the LCD is on - Mooneye's stat_lyc_onoff.gb
            // (test_roms/mooneye/) shows the comparison clock is frozen
            // while the LCD is off, so a mid-off LYC write must not
            // retrigger it (that only happens on the next LCD-on
            // transition, above).
            if (ppu->lcdc & 0x80) {
                update_lyc_flag(ppu);
                update_stat_line(ppu, cpu);
            }
            break;
        case 0xFF46:
            // A real, timed 160 M-cycle transfer (pandocs'
            // OAM_DMA_Transfer.md), not an instant copy - see cpu.h's
            // GBCpu.dma_request_pending/gb_dma_tick() (mmu.c) for the
            // actual per-M-cycle state machine this only *schedules*
            // here. `ppu->dma` reflects the value immediately on write,
            // for simple register readback - Mooneye's own reference
            // model (mooneye-gb) instead only updates its equivalent
            // field once the transfer actually starts (2 M-cycles
            // later), a finer distinction that only matters for
            // *reading* $FF46 back mid-request, which none of this
            // project's committed Mooneye ROMs test.
            ppu->dma = val;
            cpu->dma_request_pending = 1;
            cpu->dma_request_value = val;
            break;
        case 0xFF47: ppu->bgp = val; break;
        case 0xFF48: ppu->obp0 = val; break;
        case 0xFF49: ppu->obp1 = val; break;
        case 0xFF4A: ppu->wy = val; break;
        case 0xFF4B: ppu->wx = val; break;
        default: break;
    }
}
