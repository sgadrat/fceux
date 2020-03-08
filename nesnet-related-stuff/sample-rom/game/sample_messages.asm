sample_messages_init:
.(
	.(
		; Point PPU to Background palette 0 (see http://wiki.nesdev.com/w/index.php/PPU_palettes)
		lda PPUSTATUS
		lda #$3f
		sta PPUADDR
		lda #$00
		sta PPUADDR

		; Write palette_data in actual ppu palettes
		ldx #$00
		copy_palette:
		lda palettes_data, x
		sta PPUDATA
		inx
		cpx #$20
		bne copy_palette

		; Copy background from PRG-rom to PPU nametable
		lda #<nametable_data
		sta tmpfield1
		lda #>nametable_data
		sta tmpfield2
		jsr draw_zipped_nametable

		; Wait for ESP to be ready
		jsr wait_esp_reset

		rts
	.)

palettes_data:
; Background
.byt $20,$0d,$20,$0d, $20,$16,$1a,$00, $20,$0d,$0d,$0d, $20,$0d,$0d,$0d
; Sprites
.byt $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10

nametable_data:
.byt ZIPNT_ZEROS(32*3+12)
.byt                                                                $10, $08, $16, $16,  $04, $0a, $08, $16
.byt ZIPNT_ZEROS(12+32*3)
.byt ZIPNT_ZEROS(32*4)
;    -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------
.byt ZIPNT_ZEROS(18+32*6)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPNT_ZEROS(8*8)
.byt ZIPNT_END

.)

sample_messages_tick:
.(
	; Do nothing until an input is confirmed (button pressed, then released)
	lda controller_a_btns
	bne end
	cmp controller_a_last_frame_btns
	beq end

#if 1
		; Send a message in medium format
		lda #<(med_msg+1)
		sta tmpfield1
		lda #>(med_msg+1)
		sta tmpfield2

		ldx med_msg

		jsr send_med_msg

		; Send a message in long format
		lda #<(long_msg+1)
		sta tmpfield1
		lda #>(long_msg+1)
		sta tmpfield2

		ldx long_msg

		lda #1

		jsr send_long_msg
#endif

	end:
	rts

	med_msg:
		.asc 10, "short msg", $0d
	long_msg:
		.asc 24, "this one is pretty long", $0d
.)
