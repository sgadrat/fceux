SAMPLE_UDP_BANK = CURRENT_BANK_NUMBER

sample_udp_screen_init:
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

		; Create UDP "connection"
		ESP_SEND_CMD(set_udp_cmd)
		ESP_SEND_CMD(set_localhost_1234_cmd)
		ESP_SEND_CMD(connect_cmd)

		rts
	.)

set_udp_cmd:
.byt 2, TOESP_MSG_SERVER_SET_PROTOCOL, ESP_PROTOCOL_UDP
set_localhost_1234_cmd:
.byt 12, TOESP_MSG_SERVER_SET_SETTINGS, >1234, <1234, "localhost"
connect_cmd:
.byt 1, TOESP_MSG_SERVER_CONNECT

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

sample_udp_screen_tick:
.(
	; Reset drawn nametable buffers
	jsr reset_nt_buffers

	; Show data received from server
	jsr sample_udp_receive_msg

	; Show ESP states
	jsr sample_udp_show_connection_state

	; When player presses a button, send a message to the server
	lda controller_a_last_frame_btns
	bne end

	lda controller_a_btns
	beq end

	jsr sample_udp_screen_send_msg

	end:
	rts
.)

sample_udp_screen_send_msg:
.(
	; Send unknown message, mapper should ignore it
	lda #1
	sta $5000
	lda #42
	sta $5000

	; Actually send a message for the server
	lda #15   ; Message length
	sta $5000 ;

	lda #TOESP_MSG_SERVER_SEND_MESSAGE    ; Message type - message for the server
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

sample_udp_show_connection_state:
.(
	wifi_state = tmpfield1
	server_state = tmpfield2

	; Fetch wifi state
	lda #1                         ;
	sta $5000                      ;
	lda #TOESP_MSG_WIFI_GET_STATUS ; Send wifi status request to ESP
	sta $5000                      ;

	.(               ;
		wait_esp:    ;
		bit $5001    ; Wait for ESP answer
		bpl wait_esp ;
	.)               ;

	lda $5000 ; Garbage byte
	lda $5000 ; ESP message length (should be 3)
	lda $5000 ; ESP message type (should be FROMESP_MSG_WIFI_STATUS)
	lda $5000      ; Fetch ESP response
	cmp #3              ;
	beq green           ;
		lda #0          ;
		jmp store_state ; Convert response to 0=bad, 1=good
	green:              ;
		lda #1          ;
	store_state:
	sta wifi_state

	; Fetch server state
	lda #1                           ;
	sta $5000                        ;
	lda #TOESP_MSG_SERVER_GET_STATUS ; Send server status request to ESP
	sta $5000                        ;

	.(               ;
		wait_esp:    ;
		bit $5001    ; Wait for ESP answer
		bpl wait_esp ;
	.)               ;

	lda $5000 ; Garbage byte
	lda $5000 ; ESP message length (should be 3)
	lda $5000 ; ESP message type (should be FROMESP_MSG_SERVER_STATUS)
	lda $5000        ; Fetch ESP response
	sta server_state ;

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
	lda wifi_state
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
	lda server_state
	clc
	adc #1
	sta nametable_buffers+4, x
	lda #0
	sta nametable_buffers+5, x

	rts
.)

sample_udp_receive_msg:
.(
	nb_chars = tmpfield1

	; Check if there is data ready
	bit $5001
	bpl end

		; Burn ESP header
		lda $5000 ; Gabage byte
		lda $5000 ; message length
		lda $5000 ; message type

		; Prepare the nametable buffer be drawn on screen
		;  Continuation | PPU Address | Size                   | Data           | Next continuation
		;  $01          | $2204       | nb bytes read from esp | bytes from esp | $00
		jsr last_nt_buffer
		lda #1
		sta nametable_buffers, x
		lda #$22
		sta nametable_buffers+1, x
		lda #$04
		sta nametable_buffers+2, x

		lda #0
		sta nb_chars

		txa
		tay
		copy_one_byte_from_esp:
			bit $5001
			bpl end_copy

			lda $5000
			sec      ; Convert ascii to our alphatical tiles index
			sbc #$5d ;
			sta nametable_buffers+4, x
			inx

			inc nb_chars
			jmp copy_one_byte_from_esp
		end_copy:

		lda #0
		sta nametable_buffers+4, x
		tya
		tax
		lda nb_chars
		sta nametable_buffers+3, x

	end:
	rts
.)
