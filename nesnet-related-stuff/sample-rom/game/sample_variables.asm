sample_variables_incoming_current_line = $0400

sample_variables_init:
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

		; Setup state
		lda #0
		sta sample_variables_incoming_current_line

		; Wait for ESP to be ready
		jsr wait_esp_reset

		; Setup connection 0
		LONG_MSG(msg_ip_0, #$0f)
		LONG_MSG(msg_port_0, #$0f)
		LONG_MSG(msg_proto_0, #$0f)

		; Setup connection 1
		LONG_MSG(msg_ip_1, #$0f)
		LONG_MSG(msg_port_1, #$0f)
		LONG_MSG(msg_proto_1, #$0f)

		rts
	.)

msg_ip_0:
	.byt $0b, $10, $00, "127.0.0.1" ; size (11 bytes), sub-opcode (modify conn #0), property (IP addr), value
msg_port_0:
	.byt $04, $10, $01, <1234, >1234
msg_proto_0:
	.byt $03, $10, $02, 0

msg_ip_1:
	.byt $0b, $11, $00, "127.0.0.1" ; size (11 bytes), sub-opcode (modify conn #0), property (IP addr), value
msg_port_1:
	.byt $04, $11, $01, <1235, >1235
msg_proto_1:
	.byt $03, $11, $02, 0

palettes_data:
; Background
.byt $20,$0d,$20,$0d, $20,$16,$1a,$00, $20,$0d,$0d,$0d, $20,$0d,$0d,$0d
; Sprites
.byt $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10

nametable_data:
.byt ZIPNT_ZEROS(32*3+12)
.byt                                                                $19, $04, $15, $0c,  $04, $05, $0f, $08,  $16
.byt ZIPNT_ZEROS(11+32*3)
.byt ZIPNT_ZEROS(32*4)
;    -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------  -------------------
.byt ZIPNT_ZEROS(18+32*6)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPNT_ZEROS(8*8)
.byt ZIPNT_END

.)

sample_variables_tick:
;.(
;	lda sample_variables_incoming_current_line
;	bne end
;		jmp post_end
;	end:
;	rts
;	post_end:
;.)
.(
	jsr reset_nt_buffers

	; Draw one line of the incoming variables
	.(
		jsr last_nt_buffer

		; Write nt buffer header
		lda #$01 ; Continuation
		sta nametable_buffers, x
		inx

		ldy sample_variables_incoming_current_line
		lda line_ppu_addr_msb, y ; PPU address MSB
		sta nametable_buffers, x
		inx

		lda line_ppu_addr_lsb, y ; PPU address LSB
		sta nametable_buffers, x
		inx

		lda #24 ; Number of tiles
		sta nametable_buffers, x
		inx

		; Copy tiles in nt buffer
		ldy #0
		copy_one_variable:
			tya
			pha

			; Read variable "line*8 + y"
			sta tmpfield1
			lda sample_variables_incoming_current_line
			asl
			asl
			asl
			clc
			adc tmpfield1
			jsr rd_esp_var
			sta tmpfield1

			; High nibble 
			lsr
			lsr
			lsr
			lsr
			jsr nibble_to_hex_tile
			sta nametable_buffers, x
			inx

			; Low nibble
			lda tmpfield1
			and #$0f
			jsr nibble_to_hex_tile
			sta nametable_buffers, x
			inx

			; Space
			lda #$ff
			sta nametable_buffers, x
			inx

			; Loop
			pla
			tay
			iny
			cpy #8
			bne copy_one_variable

		; Close nt buffer
		lda #$00
		sta nametable_buffers, x

		; Increment current line (so the next frame, the next line will be drawn)
		ldy sample_variables_incoming_current_line
		iny
		cpy #8
		bne set_line
			ldy #0
		set_line:
		sty sample_variables_incoming_current_line
	.)

	; On confirmed input write a variable
	.(
		lda controller_a_btns
		bne end
		lda controller_a_last_frame_btns
		beq end

			; Write current line number in variable #7
			lda #7
			ldx sample_variables_incoming_current_line
			jsr wr_esp_var

		end:
	.)

	rts

	line_ppu_addr_msb:
	.byt $20, $20, $21, $21, $21, $21, $21, $21
	line_ppu_addr_lsb:
	.byt $c4, $e4, $04, $24, $44, $64, $84, $a4

	nibble_to_hex_tile:
	.(
		cmp #$0a
		bcc number
			clc
			adc #$fa
			jmp end
		number:
			clc
			adc #$1e
		end:
		rts
	.)
.)
