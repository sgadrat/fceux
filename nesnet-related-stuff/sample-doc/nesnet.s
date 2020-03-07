

;bit $5001	;Wreg->Nflag Rreg->Vflag
;bpl @wait_command  ;branch if Wclear
;bvs @wait_data  ;branch if Rset
.macro bws label ;W set ready for $5000 write
	bmi label
.endmacro
.macro bwc label ;W clear NOT ready for $5000 write
	bpl label
.endmacro
.macro brs label ;R set requested data NOT present in $5000
	bvs label
.endmacro
.macro brc label ;R clear requested data present in $5000
	bvc label
.endmacro

ESP_DATA = $5000
ESP_STATUS = $5001

;//bit7: W/R 1-write 0-read
;#define WR_CMD  0x80
NN_WR	= %10000000
NN_RD	= %00000000  ;default ready from message buffer cmd too

;//bit6: quick/immediate==1 access 2x64Bytes of vars/regs
;#define VARIABLE_CMD    0x40
NN_VAR	= %01000000

;//bit5: long/rainbow==1 next byte contains length
;#define MED_LEN_MASK  0x0F
;//      med==0 length in lower nibble
;#define RAINBOW_CMD 0x20
NN_RAIN	= %00100000
NN_MED	= %00000000

;//bit4: special==1 command in lower nibble
;//      the lengths are pre-defined
;#define SPECIAL_CMD 0x10
NN_SPEC	= %00010000
NN_SFAST= %00001000 ;//ESP responds to these commands quicker
	;16 commands possible using lower nibble
	;SLOW COMMANDS
	NN_RESET_CMD	= NN_SPEC+0;SLOW
		NN_RESET_VAL = $A5
	NN_MARK_READ	= NN_SPEC+1;SLOW

	;FAST COMMANDS
	NN_MSG_POLL	= NN_SPEC+1+NN_SFAST
	NN_MSG_SENT	= NN_SPEC+2+NN_SFAST


;only upload gamepad if it changed
send_gamepad1:

	lda gamepad
	cmp gamepad_last
	beq @done
		;gamepad changed, send the data.
		lda #0 		;var number 0-63
		;lda #63 		;var number
		ldx gamepad 	;data
		jsr wr_esp_var

	@done:

rts ; send_gamepad1:

;return:
;A= gamepad2
get_pad2_online:

	lda #0 ;var number 0-63
	jsr rd_esp_var
	;A= data on return

rts ; get_pad2_online:

;A= outgoing variable number to update 0-63
;X= data value to write
;Y unused
wr_esp_var:

	;wait $5001.7 set
	@wait_command:
		bit ESP_STATUS	;Wreg->Nflag Rreg->Vflag
		bwc @wait_command  ;branch if Wclear

	;write command
	ora #(NN_WR+NN_VAR)	;Wr-b7 varmode-b6 var#:b5-0
	sta ESP_DATA

	;wait $5001.7 set
	@wait_data:
		bit ESP_STATUS	;Wreg->Nflag Rreg->Vflag
		bwc @wait_data  ;branch if Wclear

	;write data
	stx ESP_DATA

rts ;wr_esp_var:


;A= incoming variable number to fetch 0-63
;X unused
;Y unused
;return:
;A requested variable value (data)
rd_esp_var:

	;wait $5001.7 set
	@wait_command:
		bit ESP_STATUS	;Wreg->Nflag Rreg->Vflag
		bwc @wait_command  ;branch if Wclear

	;write command
	ora #(NN_RD+NN_VAR)	;Wr-b7 varmode-b6 var#:b5-0
	sta ESP_DATA

	;wait $5001.6 clear
	@wait_data:
		bit ESP_STATUS	;Wreg->Nflag Rreg->Vflag
		brs @wait_data  ;branch if Rset

	;read data
	lda ESP_DATA

rts ;rd_esp_var:

;Call when powering up, if RESET_VAL isn't returned, the ESP isn't running yet, or it's locked up.?
;TODO ensure that command gets written the first time, and NN_RESET_VAL didn't happen to already be in the SPI reg
;RETURN A:
;	if successful, the NN_RESET_VAL
;	if failed, return command sent
reset_esp:
	;check if last frame was successful
	lda ESP_DATA
	cmp #NN_RESET_VAL
	bne @send_reset
		rts

	@send_reset:
	;don't bother waiting for W to be set, could create infinite loop

	lda #(NN_RESET_CMD)
	sta ESP_DATA

rts ;reset_esp


;ptr0 -> message to send (pascal style string first byte is length)
;X = length in bytes 1-15 bytes, 0 currently invalid (could be used for special case)
;medium messages are always connection #0
send_med_msg:

	@wait_command:
		bit ESP_STATUS
		bwc @wait_command

	;write command
	txa
	ora #(NN_WR+NN_MED)
	sta ESP_DATA

	;clear message index
	ldy #0

	@write_data:

		;load data
		lda (ptr0), Y

		@wait_data:
			bit ESP_STATUS
			bwc @wait_data

		;write data
		sta ESP_DATA

		iny ;index
		dex ;count
		bne @write_data

rts ;send_med_msg

;ptr0 -> message to send (pascal style string first byte is length)
;X = length in bytes 1-255 bytes, 0 currently invalid (could be used for special case)
;A = connection # (0-15)
send_long_msg:

	@wait_command:
		bit ESP_STATUS
		bwc @wait_command

	;write command
	ora #(NN_WR+NN_RAIN)
	sta ESP_DATA

	;write length
	@wait_length:
		bit ESP_STATUS
		bwc @wait_length

	txa
	sta ESP_DATA

	;clear message index
	ldy #0

	@write_data:

		;load data
		lda (ptr0), Y

		@wait_data:
			bit ESP_STATUS
			bwc @wait_data

		;write data
		sta ESP_DATA

		iny ;index
		dex ;count
		bne @write_data

rts ;send_med_msg

;ptr0 -> location to store message
;X = number of bytes to get
esp_get_msg:

	ldy #0 ;index

	@read_data:

		;command
		lda #NN_RD

		@wait_command:
			bit ESP_STATUS
			bwc @wait_command
	
		;write command
		sta ESP_DATA

		;TODO move slower code here while waiting for ESP to fetch data

		@wait_data:
			bit ESP_STATUS
			brs @wait_data

		;fetch data
		lda ESP_DATA

;TODO remove
;ascii offset
sub #$23

		;store data
		sta (ptr0), Y

		iny ;index
		dex ;count
		bne @read_data

;TODO remove
;null terminate
lda #0
sta (ptr0), Y

rts ;esp_get_msg

;arg: A= command to send
;ret: A= reply
send_cmd_get_reply:

	@wait_command:
		bit ESP_STATUS
		bwc @wait_command

	;poll number of messages
	;lda #COMMAND arg:A
	sta ESP_DATA

	@wait_reply:
		bit ESP_STATUS
		brs @wait_reply

	lda ESP_DATA ;number of messages

rts ; send_cmd_get_reply

;arg: A= command to send
;ret: A= reply
send_cmd_no_reply:

	@wait_command:
		bit ESP_STATUS
		bwc @wait_command

	;poll number of messages
	;lda #COMMAND arg:A
	sta ESP_DATA

rts ; send_cmd_get_reply

