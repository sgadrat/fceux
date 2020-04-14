MENU_BANK = CURRENT_BANK_NUMBER

menu_selected_option = $0400

menu_screen_init:
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

		; Reset screen's state
		lda #0
		sta menu_selected_option

		rts
	.)

palettes_data:
; Background
.byt $20,$0d,$20,$0d, $20,$16,$1a,$00, $20,$0d,$0d,$0d, $20,$0d,$0d,$0d
; Sprites
.byt $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10

nametable_data:
.byt ZIPNT_ZEROS(32*3+10)
.byt                                                     $07, $08,  $10, $12, $ff, $ff,  $16, $08, $0f, $08,  $06, $17
.byt ZIPNT_ZEROS(10+32*2+4)
.byt                      $11, $12, $ff, $0c,  $15, $14, $ff, $16,  $04, $10, $13, $0f,  $08
.byt ZIPNT_ZEROS(15+4)
.byt                      $0c, $15, $14, $ff,  $16, $04, $10, $13,  $0f, $08
.byt ZIPNT_ZEROS(18+4)
.byt                      $09, $0c, $0f, $08,  $16, $ff, $16, $04,  $10, $13, $0f, $08
.byt ZIPNT_ZEROS(16+4)
.byt                      $06, $12, $11, $11,  $08, $06, $17, $0c,  $12, $11, $ff, $16,  $04, $10, $13, $0f,  $08
.byt ZIPNT_ZEROS(11+4)
.byt                      $18, $07, $13, $ff,  $16, $04, $10, $13,  $0f, $08
.byt ZIPNT_ZEROS(18+4)
.byt                      $05, $18, $09, $09,  $08, $15, $ff, $07,  $15, $12, $13
;    -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------
.byt ZIPNT_ZEROS(17+32*5)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPNT_ZEROS(8*8)
.byt ZIPNT_END

.)

menu_screen_tick:
.(
	; Check input
	lda controller_a_last_frame_btns
	bne end_check_input

	lda controller_a_btns
	beq end_check_input
	cmp #CONTROLLER_BTN_START
	beq start_sample

		inc menu_selected_option
		lda menu_selected_option
		cmp #NB_OPTIONS
		bne end_check_input
			lda #0
			sta menu_selected_option
			jmp end_check_input

	start_sample:

		ldx menu_selected_option
		lda options_value, x
		jsr change_global_game_state

	end_check_input:

	; Place sprite
	lda menu_selected_option
	asl
	asl
	asl
	clc
	adc #48
	sta oam_mirror+0 ; Y = 48 + 8 * selected_option

	lda #0
	sta oam_mirror+1 ; TILE = arrow
	sta oam_mirror+2 ; ATTRIBUTES = 0

	lda #16
	sta oam_mirror+3 ; X

	end:
	rts

NB_OPTIONS = 6
options_value:
.byt 0, 1, 2, 3, 4, 6
.)
