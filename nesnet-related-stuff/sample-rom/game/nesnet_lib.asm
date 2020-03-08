;bit $5001  ;Wreg->Nflag Rreg->Vflag
;bpl @wait_command  ;branch if Wclear
;bvs @wait_data  ;branch if Rset
#define BWS(label) bmi label ;W set ready for $5000 write
#define BWC(label) bpl label ;W clear NOT ready for $5000 write
#define BRS(label) bvs label ;R set requested data NOT present in $5000
#define BRC(label) bvc label ;R clear requested data present in $5000

ESP_DATA = $5000
ESP_STATUS = $5001

;//bit7: W/R 1-write 0-read
;#define WR_CMD  0x80
NN_WR   = %10000000
NN_RD   = %00000000  ;default ready from message buffer cmd too

;//bit6: quick/immediate==1 access 2x64Bytes of vars/regs
;#define VARIABLE_CMD    0x40
NN_VAR  = %01000000

;//bit5: long/rainbow==1 next byte contains length
;#define MED_LEN_MASK  0x0F
;//      med==0 length in lower nibble
;#define RAINBOW_CMD 0x20
NN_RAIN = %00100000
NN_MED  = %00000000

;//bit4: special==1 command in lower nibble
;//      the lengths are pre-defined
;#define SPECIAL_CMD 0x10
NN_SPEC = %00010000
NN_SFAST= %00001000 ;//ESP responds to these commands quicker
	;16 commands possible using lower nibble
	;SLOW COMMANDS
	NN_RESET_CMD    = NN_SPEC+0;SLOW
		NN_RESET_VAL = $A5
	NN_MARK_READ    = NN_SPEC+1;SLOW

	;FAST COMMANDS
	NN_MSG_POLL = NN_SPEC+1+NN_SFAST
	NN_MSG_SENT = NN_SPEC+2+NN_SFAST

;Call when powering up, if RESET_VAL isn't returned, the ESP isn't running yet, or it's locked up.?
;TODO ensure that command gets written the first time, and NN_RESET_VAL didn't happen to already be in the SPI reg
;RETURN A:
;   if successful, the NN_RESET_VAL
;   if failed, return command sent
reset_esp:
.(
	;check if last frame was successful
	lda ESP_DATA
	cmp #NN_RESET_VAL
	bne send_reset
		rts

	send_reset:
	;don't bother waiting for W to be set, could create infinite loop

	lda #(NN_RESET_CMD)
	sta ESP_DATA

	rts ;reset_esp
.)

;tmpfield1 -> message to send (pascal style string first byte is length)
;X = length in bytes 1-15 bytes, 0 currently invalid (could be used for special case)
;medium messages are always connection #0
send_med_msg:
.(
	wait_command:
		bit ESP_STATUS
		BWC(wait_command)

	;write command
	txa
	ora #(NN_WR+NN_MED)
	sta ESP_DATA

	;clear message index
	ldy #0

	write_data:

		;load data
		lda (tmpfield1), Y

		wait_data:
			bit ESP_STATUS
			BWC(wait_data)

		;write data
		sta ESP_DATA

		iny ;index
		dex ;count
		bne write_data

	rts ;send_med_msg
.)

;tmpfield1 -> message to send (pascal style string first byte is length)
;X = length in bytes 1-255 bytes, 0 currently invalid (could be used for special case)
;A = connection # (0-15)
send_long_msg:
.(
	wait_command:
		bit ESP_STATUS
		BWC(wait_command)

	;write command
	ora #(NN_WR+NN_RAIN)
	sta ESP_DATA

	;write length
	wait_length:
		bit ESP_STATUS
		BWC(wait_length)

	txa
	sta ESP_DATA

	;clear message index
	ldy #0

	write_data:

		;load data
		lda (tmpfield1), Y

		wait_data:
			bit ESP_STATUS
			BWC(wait_data)

		;write data
		sta ESP_DATA

		iny ;index
		dex ;count
		bne write_data

	rts ;send_long_msg
.)
