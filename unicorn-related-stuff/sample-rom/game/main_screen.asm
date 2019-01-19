main_screen_init:
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

		; Show ESP states
		jsr main_show_connection_state

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

main_screen_tick:
.(
	lda controller_a_last_frame_btns
	bne end

	lda controller_a_btns
	beq end

	jsr main_screen_send_msg

	end:
	rts
.)

#define MSG_NULL 0
#define MSG_GET_WIFI_STATUS 1
#define MSG_GET_SERVER_STATUS 2
#define MSG_SEND_MESSAGE 3

main_screen_send_msg:
.(
	; Send unknown message, mapper should ignore it
	lda #42
	sta $5000

	; Actually send a message for the server
	lda #MSG_SEND_MESSAGE ; Message type - message for the server
	sta $5000             ;

	lda #13   ; Message length
	sta $5000 ;

	lda #$55  ;
	sta $5000 ;
	lda #$6e  ;
	sta $5000 ;
	lda #$69  ;
	sta $5000 ;
	lda #$63  ;
	sta $5000 ;
	lda #$6f  ;
	sta $5000 ;
	lda #$72  ; Unicorn roxx!
	sta $5000 ;
	lda #$6e  ;
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

	rts
.)

main_show_connection_state:
.(
	wifi_state = tmpfield1
	server_state = tmpfield2

	; Fetch wifi state
	lda #MSG_GET_WIFI_STATUS ; Send wifi status request to ESP
	sta $5000                ;

	lda $5000      ; Fetch ESP response
	sta wifi_state ;

	; Fetch server state
	lda #MSG_GET_SERVER_STATUS ; Send server status request to ESP
	sta $5000                  ;

	lda $5000        ; Fetch ESP response
	sta server_state ;

	; Draw wifi state
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

	; Draw server state
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
