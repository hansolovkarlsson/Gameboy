; HDMA/GDMA test ROM - real-hardware-shaped validation of CGB VRAM DMA
; transfers (0xFF51-0xFF55), driven entirely by genuinely CPU-executed
; code and (for the HBlank round) genuine PPU Mode 3->0 timing, unlike
; tests/test_cpu.c's direct/synthetic gb_hdma_hblank_trigger() calls -
; see docs/GAMEBOY_ROADMAP.md's HDMA/GDMA follow-up entry and
; rgbds/README.md for the full reasoning (a search for a real,
; permissively-licensed game/demo using HDMA came up empty: tobutobugirl-dx
; never references HDMA5 at all; mills32/Parallax-effect-for-Game-Boy-Color
; genuinely does, but has no license and needs a toolchain - SDCC/GBDK -
; this project hasn't set up).
;
; Must run with --mode cgb (HDMA is CGB-only; this ROM's own header
; doesn't carry the CGB flag - gb_resolve_cgb_mode()'s "cgb" mode
; forces CGB regardless). See make gameboy-rgbds-hdma-test.
;
; Three rounds, each over VRAM ($8000+), reported over serial (SB/SC,
; the same mechanism hello.asm/mbc3_rtc.asm use):
;   1. GDMA (LCD off): one 16-byte block from ROM to VRAM bank 0 $8000.
;      Real hardware/this emulator both block the CPU until the transfer
;      finishes, so the very next instruction can safely read the bytes
;      straight back.
;   2. HBlank DMA (LCD on): two 16-byte blocks from ROM to VRAM bank 0
;      $8010, polled to completion - genuinely depends on two real
;      HBlank periods occurring (ppu.c's Mode 3->0 transition firing
;      gb_hdma_hblank_trigger()), not a direct call.
;   3. VRAM bank isolation (LCD off): GDMA 16 bytes into VRAM bank 1's
;      $8000 (same address round 1 used, different bank - VBK selects
;      it), then re-read bank 0's $8000 to confirm round 1's bytes are
;      still there, untouched - proving the two banks never
;      cross-contaminated, not just that writing to bank 1 "did something".

SECTION "Header", ROM0[$100]
    jp EntryPoint
    ds $150 - @, 0 ; Space for the header RGBFIX fills in

SECTION "Main", ROM0[$150]
EntryPoint:
    ; Guarantee the LCD starts off, rather than relying on whatever this
    ; emulator's own unbooted default happens to be - GDMA/VRAM-readback
    ; below needs it off regardless.
    xor a
    ldh [$FF40], a ; LCDC

    ; --- Round 1: GDMA, LCD off, VRAM bank 0 ---
    ld hl, Round1Label
    call PrintString
    ld a, HIGH(Round1Data)
    ldh [$FF51], a ; HDMA1: source high
    ld a, LOW(Round1Data)
    ldh [$FF52], a ; HDMA2: source low
    ld a, $80
    ldh [$FF53], a ; HDMA3: dest high ($8000)
    xor a
    ldh [$FF54], a ; HDMA4: dest low
    xor a           ; bit7=0 (GDMA), length = (0+1)*16 = 16 bytes
    ldh [$FF55], a ; HDMA5: start - blocks the CPU until this one block finishes
    ld hl, $8000
    ld b, 16
    call ReadAndSendBlock

    ; --- Round 2: HBlank DMA, LCD on, VRAM bank 0 ---
    ld hl, Round2Label
    call PrintString
    ld a, HIGH(Round2Data)
    ldh [$FF51], a
    ld a, LOW(Round2Data)
    ldh [$FF52], a
    ld a, $80
    ldh [$FF53], a ; dest high ($8010)
    ld a, $10
    ldh [$FF54], a ; dest low
    ld a, $80       ; LCD on (bit 7) - enough to start real Mode 2/3/0/1
    ldh [$FF40], a  ; transitions, no actual tile/palette setup needed
    ld a, $81       ; bit7=1 (HBlank DMA), length = (1+1)*16 = 32 bytes
    ldh [$FF55], a  ; HDMA5: start - two real HBlank periods needed to finish
.waitHdma
    ldh a, [$FF55]
    bit 7, a        ; pandocs: 1 = not active (done), 0 = still active
    jr z, .waitHdma
    xor a
    ldh [$FF40], a  ; LCD off again - safe to read VRAM directly
    ld hl, $8010
    ld b, 32
    call ReadAndSendBlock

    ; --- Round 3: VRAM bank isolation, LCD off ---
    ld hl, Round3aLabel
    call PrintString
    ld a, $01
    ldh [$FF4F], a ; VBK: select VRAM bank 1
    ld a, HIGH(Round3Data)
    ldh [$FF51], a
    ld a, LOW(Round3Data)
    ldh [$FF52], a
    ld a, $80
    ldh [$FF53], a ; dest high ($8000) - same address round 1 used, different bank
    xor a
    ldh [$FF54], a
    xor a           ; GDMA, 16 bytes
    ldh [$FF55], a
    ld hl, $8000
    ld b, 16
    call ReadAndSendBlock

    ld hl, Round3bLabel
    call PrintString
    xor a
    ldh [$FF4F], a ; VBK: back to bank 0
    ld hl, $8000
    ld b, 16
    call ReadAndSendBlock ; should be round 1's original bytes, untouched

    ld hl, DoneLabel
    call PrintString
.hang
    jr .hang

; Reads B bytes starting at HL and sends each one over serial, in order.
ReadAndSendBlock:
    ld a, [hl+]
    ld c, a
    call SerialSend
    dec b
    jr nz, ReadAndSendBlock
    ret

; Prints a null-terminated string pointed to by HL over serial.
PrintString:
    ld a, [hl+]
    and a
    ret z
    ld c, a
    call SerialSend
    jr PrintString

SerialSend:
    ld a, c
    ldh [$FF01], a ; SB
    ld a, $81
    ldh [$FF02], a ; SC: start transfer, internal clock
    ret

Round1Label:  db " R1:", 0
Round2Label:  db " R2:", 0
Round3aLabel: db " R3a:", 0
Round3bLabel: db " R3b:", 0
DoneLabel:    db " DONE", 0

; ALIGN[4] guarantees each data block starts on a 16-byte boundary -
; real hardware forces the low 4 bits of an HDMA source address to 0,
; so an unaligned label here would silently source from a few bytes
; earlier than intended.
SECTION "Round1Data", ROM0, ALIGN[4]
Round1Data: db "GDMAROUND1BYTES!"

SECTION "Round2Data", ROM0, ALIGN[4]
Round2Data: db "HBLANKROUND2BYTES0123456789ABCDE"

SECTION "Round3Data", ROM0, ALIGN[4]
Round3Data: db "BANK1-ISOLATION!"
