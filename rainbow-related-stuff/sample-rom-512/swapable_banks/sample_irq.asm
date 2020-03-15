SAMPLE_IRQ_BANK = CURRENT_BANK_NUMBER

sample_irq_last_server_msg = $0400
sample_irq_wifi_state = $10
sample_irq_server_state = $11

sample_irq_screen_init:
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

		; Reset demo state
		lda #0
		sta sample_irq_last_server_msg
		sta sample_irq_wifi_state
		sta sample_irq_server_state

		; Unmask IRQs
		cli

		; Connect to server
		ESP_SEND_CMD(connect_cmd)

		rts
	.)

connect_cmd:
.byt 1, TOESP_MSG_CONNECT_TO_SERVER

palettes_data:
; Background
.byt $20,$0d,$20,$0d, $20,$16,$1a,$00, $20,$0d,$0d,$0d, $20,$0d,$0d,$0d
; Sprites
.byt $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10, $20,$0d,$00,$10

nametable_data:
.byt ZIPNT_ZEROS(32*3+4)
.byt                      $1a, $0c, $09, $0c,  $03, ZIPNT_ZEROS(3+4+4),                                       $16, $08, $15, $19,  $08, $15, $03
.byt ZIPNT_ZEROS(5+32*6)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPZ, ZIPZ, %00010000, ZIPZ, ZIPZ, ZIPZ, %01000000, ZIPZ
.byt ZIPNT_ZEROS(8*7)
.byt ZIPNT_END

.)

sample_irq_process_irq:
.(
	.(
		pha
		txa
		pha
		tya
		pha

		process_one_esp_message:
			lda $5000 ; Garbage byte

			lda $5000 ; Message length
			tax       ;

			lda $5000 ; Message type
			dex       ;

			tay                         ;
			lda process_routines_lsb, y ;
			sta tmpfield1               ;
			lda process_routines_msb, y ; Call the subroutine for this message
			sta tmpfield2               ;
			jsr call_pointed_subroutine ;

			bit $5001                   ; Loop until there is no more data in ESP's buffer
			bmi process_one_esp_message ;

		end:
			pla
			tay
			pla
			tax
			pla
			rti
	.)

	process_wifi_state:
	.(
		lda $5000
		cmp #3
		beq green
			lda #0
		green:
			lda #1

		store_state:
		sta sample_irq_wifi_state
		rts
	.)

	process_server_state:
	.(
		lda $5000
		sta sample_irq_server_state
		rts
	.)

	process_server_message:
	.(
		; Copy message size before the actual message
		stx sample_irq_last_server_msg

		; Copy message payload
		ldy #1
		copy_one_byte:
			cpx #0
			beq end
			dex

			lda $5000
			sta sample_irq_last_server_msg, y
			iny

			jmp copy_one_byte

		end:
			rts
	.)

	process_routines_lsb:
		.byt <dummy_routine
		.byt <dummy_routine
		.byt <dummy_routine
		.byt <dummy_routine
		.byt <process_wifi_state
		.byt <process_server_state
		.byt <process_server_message

	process_routines_msb:
		.byt >dummy_routine
		.byt >dummy_routine
		.byt >dummy_routine
		.byt >dummy_routine
		.byt >process_wifi_state
		.byt >process_server_state
		.byt >process_server_message
.)

sample_irq_screen_tick:
.(
	; Reset drawn nametable buffers
	jsr reset_nt_buffers

	; Show data received from server
	jsr sample_irq_show_msg

	; Show ESP states
	jsr sample_irq_show_connection_state

	; Ask ESP to send connection state info
	lda #1                         ;
	sta $5000                      ;
	lda #TOESP_MSG_GET_WIFI_STATUS ; Send wifi status request to ESP
	sta $5000                      ;
	lda #1                           ;
	sta $5000                        ;
	lda #TOESP_MSG_GET_SERVER_STATUS ; Send server status request to ESP
	sta $5000                        ;

	; When player presses a button, send a message to the server
	lda controller_a_last_frame_btns
	bne end

	lda controller_a_btns
	beq end

	jsr sample_irq_screen_send_msg

	end:
	rts
.)

sample_irq_screen_send_msg:
.(
	; Send unknown message, mapper should ignore it
	lda #1
	sta $5000
	lda #42
	sta $5000

	; Actually send a message for the server
	lda #15   ; Message length
	sta $5000 ;

	lda #TOESP_MSG_SEND_MESSAGE_TO_SERVER ; Message type - message for the server
	sta $5000                             ;

	lda #$52  ;
	sta $5000 ;
	lda #$61  ;
	sta $5000 ;
	lda #$69  ;
	sta $5000 ;
	lda #$6e  ;
	sta $5000 ;
	lda #$62  ;
	sta $5000 ;
	lda #$6f  ;
	sta $5000 ; Rainbow roxx!\n
	lda #$77  ;
	sta $5000 ;
	lda #$20  ;
	sta $5000 ;
	lda #$72  ;
	sta $5000 ;
	lda #$6f  ;
	sta $5000 ;
	lda #$78  ;
	sta $5000 ;
	lda #$78  ;
	sta $5000 ;
	lda #$21  ;
	sta $5000 ;
	lda #$0a  ;
	sta $5000 ;

	rts
.)

sample_irq_show_connection_state:
.(
	; Construct command to show wifi state on screen
	;  Continuation | PPU Address | Size | Data    | Next continuation
	;  $01          | $2068       | $01  | <state> | $00
	jsr last_nt_buffer
	lda #1
	sta nametable_buffers, x
	lda #$20
	sta nametable_buffers+1, x
	lda #$68
	sta nametable_buffers+2, x
	lda #1
	sta nametable_buffers+3, x
	lda sample_irq_wifi_state
	clc
	adc #1
	sta nametable_buffers+4, x
	lda #0
	sta nametable_buffers+5, x

	; Construct command to show server state
	jsr last_nt_buffer
	lda #1
	sta nametable_buffers, x
	lda #$20
	sta nametable_buffers+1, x
	lda #$7a
	sta nametable_buffers+2, x
	lda #1
	sta nametable_buffers+3, x
	lda sample_irq_server_state
	clc
	adc #1
	sta nametable_buffers+4, x
	lda #0
	sta nametable_buffers+5, x

	rts
.)

sample_irq_show_msg:
.(
	nb_chars = sample_irq_last_server_msg

	; Prepare the nametable buffer be drawn on screen
	;  Continuation | PPU Address | Size                | Data    | Next continuation
	;  $01          | $2204       | nb bytes in message | message | $00
	jsr last_nt_buffer
	lda #1
	sta nametable_buffers, x
	lda #$22
	sta nametable_buffers+1, x
	lda #$04
	sta nametable_buffers+2, x
	lda nb_chars
	sta nametable_buffers+3, x

	ldy #0
	copy_one_byte_from_message:
		cpy nb_chars
		beq end_copy

		lda sample_irq_last_server_msg+1, y
		sec      ; Convert ascii to our alphatical tiles index
		sbc #$5d ;
		sta nametable_buffers+4, x
		inx

		iny
		jmp copy_one_byte_from_message
	end_copy:

	lda #0
	sta nametable_buffers+4, x

	end:
	rts
.)
