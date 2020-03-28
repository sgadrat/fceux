SAMPLE_FILES_BANK = CURRENT_BANK_NUMBER

sample_files_file_exists = $0400 ; 64 bytes buffer, each byte is set to 1 if the file exists

sample_files_init:
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

		rts
	.)

palettes_data:
; Background
.byt $20,$0d,$20,$0d, $20,$16,$1a,$00, $20,$0d,$0d,$0d, $20,$0d,$0d,$0d
; Sprites
.byt $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10

nametable_data:
.byt ZIPNT_ZEROS(32*4)
.byt ZIPNT_ZEROS(32*5)
.byt $ff, $ff, $ff, $ff,  $09, $0c, $0f, $08,  $ff, $04, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff
.byt $ff, $ff, $ff, $ff,  $09, $0c, $0f, $08,  $ff, $05, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff
.byt $ff, $ff, $ff, $ff,  $09, $0c, $0f, $08,  $ff, $06, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff,  $ff, $ff, $ff, $ff
.byt ZIPNT_ZEROS(32*5)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPNT_ZEROS(8*8)
.byt ZIPNT_END
.)

sample_files_screen_tick:
.(
	.(
		; Reset drawn nametable buffers
		jsr reset_nt_buffers

		; When player presses a button, modify data in PRG
		lda controller_a_last_frame_btns
		bne end

		lda controller_a_btns
		beq end

		jsr refresh_files_status
		jsr show_files
		jsr write_file

		end:
		rts
	.)

	refresh_files_status:
	.(
		; Reset all files to "inexistant"
		lda #0
		ldx #64
		reset_one_byte:
			dex
			sta sample_files_file_exists, x
			bne reset_one_byte

		; Ask ESP about existing files
		ESP_SEND_CMD(cmd_get_list)

		; Wait ESP response
		jsr esp_wait_message

		; Set listed files as existant
		lda $5000 ; Garbage byte
		nop
		lda $5000 ; Message length
		nop
		ldy $5000 ; Message type
		nop
		ldx $5000 ; Number of files

		lda #1
		set_one_file:
			cpx #0
			beq end_set_one_file

			ldy $5000
			sta sample_files_file_exists, y

			dex
			jmp set_one_file
		end_set_one_file:

		rts

		cmd_get_list:
		.byt 2, TOESP_MSG_FILE_GET_LIST, ESP_FILE_PATH_USER
	.)

	show_files:
	.(
		cur_file = tmpfield16

		lda 0
		sta cur_file

		show_one_file:
			; Check if file exists
			ldx cur_file
			lda sample_files_file_exists, x
			bne file_exists
			jmp next_file
			file_exists:

			; Select file
			lda #3
			sta $5000
			lda #TOESP_MSG_FILE_OPEN
			sta $5000
			lda #ESP_FILE_PATH_USER
			sta $5000
			lda cur_file
			sta $5000

			; Read file
			lda #2
			sta $5000
			lda #TOESP_MSG_FILE_READ
			sta $5000
			lda #3
			sta $5000

			; Wait ESP response
			jsr esp_wait_message

			; Construct nametable buffer from file data
			;  Continuation | PPU Address           | Size | Data | Next continuation
			;  $01          | $212c + 32 * cur_file | $03  | data | $00
			lda $5000 ; Garbage byte
			nop
			lda $5000 ; Message length
			nop
			lda $5000 ; Message type
			nop
			lda $5000 ; Data length

			lda #$21
			sta tmpfield1
			lda #$2c
			sta tmpfield2
			ldx cur_file
			add_file_offset:
				cpx #0
				beq end_add_file_offset

				clc
				lda tmpfield2
				adc #32
				sta tmpfield2
				lda tmpfield1
				adc #0
				sta tmpfield1

				dex
				jmp add_file_offset
			end_add_file_offset:
			
			jsr last_nt_buffer
			lda #$01
			sta nametable_buffers, x
			lda tmpfield1
			sta nametable_buffers+1, x
			lda tmpfield2
			sta nametable_buffers+2, x
			lda #$03
			sta nametable_buffers+3, x
			lda $5000
			sta nametable_buffers+4, x
			lda $5000
			sta nametable_buffers+5, x
			lda $5000
			sta nametable_buffers+6, x
			lda #$0
			sta nametable_buffers+7, x

			; Loop over the three first files
			next_file:
			inc cur_file
			lda cur_file
			cmp #3
			beq end
			jmp show_one_file

		end:
		rts
	.)

	write_file:
	.(
		; Select file
		lda #3
		sta $5000
		lda #TOESP_MSG_FILE_OPEN
		sta $5000
		lda #ESP_FILE_PATH_USER
		sta $5000
		lda #2
		sta $5000

		; Write data in file
		lda #5
		sta $5000 ; Message length
		lda #TOESP_MSG_FILE_WRITE
		sta $5000 ; Command
		lda #3
		sta $5000 ; Data length
		lda #$1d
		sta $5000 ; Data
		sta $5000 ;
		sta $5000 ;

		rts
	.)
.)
