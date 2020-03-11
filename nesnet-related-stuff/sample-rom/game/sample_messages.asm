sample_messages_rcv_buffer = $400

#define LONG_MSG(data,conn) \
	lda #<(data+1) :\
	sta tmpfield1 :\
	lda #>(data+1) :\
	sta tmpfield2 :\
	ldx data :\
	lda conn :\
	jsr send_long_msg

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
	; Send some messages when an input is confirmed (button pressed, then released)
	.(
		lda controller_a_btns
		bne end
		cmp controller_a_last_frame_btns
		beq end

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

		end:
	.)

	; Check message from connexions
	.(
		lda #NN_MSG_POLL
		jsr send_cmd_get_reply
		beq end

			; Get message
			lda #<(sample_messages_rcv_buffer+1)
			sta tmpfield1
			lda #>(sample_messages_rcv_buffer+1)
			sta tmpfield2
			ldx #15
			jsr esp_get_msg

			; Send message back to connexion #0
			lda #15
			sta sample_messages_rcv_buffer
			LONG_MSG(sample_messages_rcv_buffer, #0)

		end:
	.)
	rts

	med_msg:
		.asc 10, "short msg", $0a
	long_msg:
		.asc 24, "this one is pretty long", $0a
.)
