sample_connection_screen_init:
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

sample_connection_screen_tick:
.(
	; Reset drawn nametable buffers
	jsr reset_nt_buffers

	; Show ESP states
	jsr sample_connection_show_connection_state

	; When player presses a button, send a message to the server
	lda controller_a_last_frame_btns
	bne end

	lda controller_a_btns
	beq end

	jsr sample_connection_switch_connection

	end:
	rts
.)

wifi_state = $0400
server_state = $0401

sample_connection_switch_connection:
.(
	lda server_state
	bne disconnect

		; Send connection message
		lda #1
		sta $5000
		lda #TOESP_MSG_SERVER_CONNECT
		sta $5000

		jmp end

	disconnect:

		; Send disconnection message
		lda #1
		sta $5000
		lda #TOESP_MSG_SERVER_DISCONNECT
		sta $5000

	end:
	rts
.)

sample_connection_show_connection_state:
.(
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
