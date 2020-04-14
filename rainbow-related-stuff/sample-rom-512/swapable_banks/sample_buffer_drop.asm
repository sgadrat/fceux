SAMPLE_BUFFER_DROP_BANK = CURRENT_BANK_NUMBER

sample_buffer_drop_current_line = $0400

sample_buffer_drop_init:
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

		; Init state
		lda #0
		sta sample_buffer_drop_current_line

		;
		; Get multiple messages, then drop some with E2N_BUFFER_DROP, and show result
		;

		; Ensure file is deleted before the begining
		ESP_SEND_CMD(cmd_file_delete)

		; Fill the buffer with messages
		ESP_SEND_CMD(cmd_file_exists)
		ESP_SEND_CMD(cmd_file_exists)
		ESP_SEND_CMD(cmd_file_info)
		ESP_SEND_CMD(cmd_file_exists)
		ESP_SEND_CMD(cmd_file_create)
		ESP_SEND_CMD(cmd_file_exists)
		ESP_SEND_CMD(cmd_file_delete)
		ESP_SEND_CMD(cmd_file_info)
		ESP_SEND_CMD(cmd_file_exists)

		; Prune some messages
#if 1
		ESP_SEND_CMD(cmd_buffer_drop)
#endif

		rts

	.)

cmd_file_delete:
.byt 3, TOESP_MSG_FILE_DELETE, ESP_FILE_PATH_SAVE, 5
cmd_file_exists:
.byt 3, TOESP_MSG_FILE_EXISTS, ESP_FILE_PATH_SAVE, 5
cmd_file_create:
.byt 3, TOESP_MSG_FILE_OPEN, ESP_FILE_PATH_SAVE, 5
cmd_file_info:
.byt 3, TOESP_MSG_FILE_GET_INFO, ESP_FILE_PATH_SAVE, 5
cmd_buffer_drop:
.byt 3, TOESP_MSG_E2N_BUFFER_DROP, FROMESP_MSG_FILE_EXISTS, 2

palettes_data:
; Background
.byt $20,$0d,$20,$20, $20,$16,$1a,$00, $20,$0d,$0d,$0d, $20,$0d,$0d,$0d
; Sprites
.byt $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10

nametable_data:
.byt ZIPNT_ZEROS(32*5)
.byt ZIPNT_ZEROS(32*6)
.byt ZIPNT_ZEROS(32*6)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPNT_ZEROS(8*8)
.byt ZIPNT_END
.)

sample_buffer_drop_tick:
.(
	.(
		; Reset drawn nametable buffers
		jsr reset_nt_buffers

		; If a message is comming from ESP, show it on screen
		bit rainbow_flags
		bpl end

		jsr show_message

		end:
		rts
	.)

	show_message:
	.(
		; Construct nametable buffer from file data
		;  Continuation | PPU Address           | Size         | Data    | Next continuation
		;  $01          | $2024 + 32 * cur_line | msg_size * 2 | msg_hex | $00

		; Compute PPU address
		lda #$20
		sta tmpfield1
		lda #$24
		sta tmpfield2
		ldx sample_buffer_drop_current_line
		add_line_offset:
			cpx #0
			beq end_add_line_offset

			clc
			lda tmpfield2
			adc #32
			sta tmpfield2
			lda tmpfield1
			adc #0
			sta tmpfield1

			dex
			jmp add_line_offset
		end_add_line_offset:

		; Construct buffer
#define buffer_field(n) lda n : sta nametable_buffers, x : inx
		jsr last_nt_buffer
		buffer_field(#$01)
		buffer_field(tmpfield1)
		buffer_field(tmpfield2)

		lda rainbow_data ; Garbage byte
		nop
		lda rainbow_data ; Message size
		tay
		asl
		sta nametable_buffers, x : inx

		one_message_byte:
			lda rainbow_data
			pha

			lsr
			lsr
			lsr
			lsr
			jsr nibble_to_hex
			sta nametable_buffers, x : inx

			pla
			and #$0f
			jsr nibble_to_hex
			sta nametable_buffers, x : inx

			dey
			bne one_message_byte

		buffer_field(#$00)

		; Increment line number
		inc sample_buffer_drop_current_line

		rts
	.)
.)
