sample_reset_esp_esp_state = $0400

SAMPLE_RESET_ESP_STATE_UNKNOWN = 0
SAMPLE_RESET_ESP_STATE_WARMING = 1
SAMPLE_RESET_ESP_STATE_READY = 2

sample_reset_esp_init:
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
		sta sample_reset_esp_esp_state

		rts
	.)

palettes_data:
; Background
.byt $20,$0d,$20,$0d, $20,$16,$1a,$00, $20,$0d,$0d,$0d, $20,$0d,$0d,$0d
; Sprites
.byt $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10

nametable_data:
.byt ZIPNT_ZEROS(32*3+11)
.byt                                                          $15,  $08, $16, $08, $17,  $ff, $08, $16, $13
.byt ZIPNT_ZEROS(12+32*2+4)
.byt                      $08, $16, $13, $ff,  $16, $17, $04, $17,  $08,
.byt ZIPNT_ZEROS(19+32*4)
;    -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------
.byt ZIPNT_ZEROS(18+32*6)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPNT_ZEROS(8*8)
.byt ZIPNT_END

.)

sample_reset_esp_tick:
.(
	; Clear bufferized commands for the NMI handler
	jsr reset_nt_buffers

	; Check ESP status if state is not ready, else do nothing
	lda sample_reset_esp_esp_state
	cmp #SAMPLE_RESET_ESP_STATE_READY
	beq end

		; Reset ESP
		jsr reset_esp

		; Write status on screen
		cmp #NN_RESET_VAL
		bne not_ready

			ready:
				ldy #1
				jmp write_buffer

			not_ready:
				ldy #0

		write_buffer:
			jsr last_nt_buffer
			lda #12
			sta tmpfield1

			write_one_byte:
				lda state_buffer, y
				sta nametable_buffers, x
				iny
				iny
				inx

				dec tmpfield1
				bne write_one_byte

	end:
	rts

	state_buffer:
		.byt $01, $01 ; Continuation
		.byt $20, $20 ; PPU address MSB
		.byt $d0, $d0 ; PPU address LSB
		.byt $07, $07 ; Size
		.byt $1a, $15
		.byt $04, $08
		.byt $15, $04
		.byt $10, $07
		.byt $0c, $1c
		.byt $11, $ff
		.byt $0a, $ff
		.byt $00, $00 ; End of buffers
.)
