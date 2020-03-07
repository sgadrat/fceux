
;NOTE routines using trampoline:  Y register gets stomped, along with N&Z flags due to LDY #return bank
	ram_tramp_code:
		;switch banks
		;NOTE!  This expects there to be a PRG bank table at the beginning of each PRG bank, must be page aligned
		ldy #$00 ;jump to bank
		;sty $8000 ;bank_table ;needs to be in rom (calling bank)
		sty prg_bank_table ;needs to be in rom (calling bank)
		;USE MASTER TABLE in last bank
		;jmp called_func 
		;jsr called_func 
		;jsr load_chrram 
		;jsr $8000
		jsr_far_func $8000; place holder, must be updated before using
		;TODO we could jump to the function, if the function jumps back here
		;return here when function complete
		;ldy #LAST_PRG_BANK;03 ;return bank
		;ldy #((CHR_BANK<<4)+LAST_PRG_BANK) ;CHR_BANK:return bank
		ldy #$0F ;placeholder (CHR bank0)
		;sty $8003 ;bank_table ;need to be in called bank
		;sty prg_bank_table+LAST_PRG_BANK ;bank_table ;need to be in called bank
		;sty prg_bank_table+CHR_BANK ;bank_table ;need to be in called bank
		sty prg_bank_table ;placeholder
		;USE CHR table in each bank
		rts ;back to ROM, can't replace this with a jump as don't know where we were called from

		;let's have the NMI vector jump here
		;ram_tramp_nmi:
		;read the prg_bank_id to determine currently selected bank
		;update the nmi return bank to that value
		;switch to last bank and jump to nmi handler
		;jmp nmi
		;switch back to return bank
		;return from interrupt
	ram_tramp_end:

;main routine
main:
;turn off NMI so won't trigger NMI during init
	lda #$00
	sta $2000

	;initialize the ram trampoline

	;copy trampoline code to ram
	ldx #ram_tramp_end-ram_tramp_code-1 ;counting to zero
	@copy_next:
		lda ram_tramp_code, x
		sta ram_tramp_exe, x
		dex
		bpl @copy_next

	
	; setup
	ldx #0
	:
		lda example_palette, X
		sta palette, X
		inx
		cpx #32
		bcc :-
	

	lda #1
	sta rand_seed ;init PRNG to anything but zero at startup

	;select CHR-RAM bank0
	sel_chr_bankY 0, LAST_PRG_BANK
	;load PT0&1 of first CHR-RAM bank
	ldx #8*1024/256	;number of pages
	ldy #$00	;MSB
	sty arg0 ;inc val
	lda #$01 ;tile data
	jsr ppu_clear_nt

	sel_chr_bankY 1, LAST_PRG_BANK
	;load PT0&1 of first CHR-RAM bank
	ldx #8*1024/256	;number of pages
	ldy #$00	;MSB
	sty arg0 ;inc val
	lda #$05 ;tile data
	jsr ppu_clear_nt

	sel_chr_bankY 2, LAST_PRG_BANK
	;load PT0&1 of first CHR-RAM bank
	ldx #8*1024/256	;number of pages
	ldy #$00	;MSB
	sty arg0 ;inc val
	lda #$15 ;tile data
	jsr ppu_clear_nt

	sel_chr_bankY 3, LAST_PRG_BANK
	;load PT0&1 of first CHR-RAM bank
	ldx #8*1024/256	;number of pages
	ldy #$00	;MSB
	sty arg0 ;inc val
	lda #$55 ;tile data
	jsr ppu_clear_nt

	;load all name/attr tables
	;ldx #48 ;pages of PPU address space to clear 48 * 256 = 12288 = 12KB = 8KB CHR-RAM + 4KB NAMETABLES
	;ldy #$00 ;MSB PT0 start address
	;ldx #4*1024/256
	;display all tiles in first 4 rows
	ldx #1 		;256B per PT
	ldy #$20	;NT0 start
	;lda #0	 ;add this value to each subsequent write
	lda #1 ;increment tile number for each byte of the NT, if tiles are all the same won't see a difference
		;aside from attribute tables
	sta arg0
	lda #$10	;value to write to PPU address
	jsr ppu_clear_nt

	;clear rest of name/attr table
	ldx #3 		;256B per PT
	ldy #$21	;1/4 down NT0
	lda #0	 ;add this value to each subsequent write
	sta arg0
	lda #' '	;value to write to PPU address (space/blank char)
	jsr ppu_clear_nt

	;load text into first PT0 first bank
	sel_chr_bankY 0, LAST_PRG_BANK
;	lda #<alphanum_64_tiles
;	sta ptr0_lo	;ptr lo
;	lda #>alphanum_64_tiles
;	sta ptr0_hi	;ptr hi
	;use a macro:
	mva_ptr ptr0, alphanum_64_tiles
	ldx #((alphanum_64_tiles_end-alphanum_64_tiles)/256) ;number of 256B pages to copy
	;ldy #$0C ;PPU MSB
	ldy #(TILE_BASE>>4) ;PPU_MSB
	jsr ppu_load_file


	;enable NMI, 8x8 sprites, BG PT0, SP PT0, NT0, VRAM INC+1
	lda #$80
	sta $2000

	;can now draw with nmt_update, but can only draw so much per frame

	;rom title
TITLE_ROW = 9
	mva_ptr ptr0, title_string
	ldx #4
	ldy #TITLE_ROW
	jsr ppu_write_string

	;controller state
CTLR_ROW = 10
	mva_ptr ptr0, ctlr_string
	ldx #4
	ldy #CTLR_ROW
	jsr ppu_write_string

	;instructions
INSTR_ROW = 11
	mva_ptr ptr0, instruction_str
	ldx #4
	ldy #INSTR_ROW
	jsr ppu_write_string

	;draw lines above
	jsr ppu_update

	;poke peek
APTR0_ROW = 12
APTR1_ROW = APTR0_ROW+1
APTR2_ROW = APTR0_ROW+2
APTR3_ROW = APTR0_ROW+3
APTR_ADDR_HI = 5
APTR_ADDR_LO = 7
APTR_WR_VAL = 11
APTR_RD_VAL = 16
	mva_ptr ptr0, test_rw_str
	ldx #4
	ldy #APTR0_ROW
	jsr ppu_write_string

	ldx #4
	ldy #APTR1_ROW
	jsr ppu_write_string

	;draw lines above
	jsr ppu_update

	ldx #4
	ldy #APTR2_ROW
	jsr ppu_write_string

	ldx #4
	ldy #APTR3_ROW
	jsr ppu_write_string

	;draw lines above
	jsr ppu_update



;	lda #$C3
;	ldx #8
;	ldy #10
;	jsr display_hex_byte
;
;	mva_ptr ptr0, $FFFF
;	ldx #8
;	ldy #11
;	jsr ptr0_peek_display


	;TODO clear all CHR-RAM banks

;	ldy #$24 ;NT1
;	lda #1	 ;tile number
;	jsr ppu_clear_nt
;	lda #$28 ;NT2
;	ldy #2	 ;tile number
;	jsr ppu_clear_nt
;	lda #$2C ;NT3
;	ldy #3	 ;tile number
;	jsr ppu_clear_nt

	;load up CHR-RAM


	;switch to the pattern table bank

	
	;jump to our CHR-RAM loading routine through the ram trampoline
;	jsr_far PT_BANK, load_chrram;, 0



	;determine what TV system/console we're running on
	jsr getTVSystem
	; @return A: TV system (-10: NTSC, -9: PAL, -8: Dendy
	cmp #NTSC
	bne :+
		adc #-NTSC+4 ;carry is set so it'll be 5 now
	:
	sta skip_frame	 ;initial value


;	;setup sprite 0
;	lda #22	;y	bottom row is aligned with HUD line
;	sta oam+(0*4)+0	
;	lda #0	;tile num
;	sta oam+(0*4)+1	
;	lda #$03	;front of BG attr
;	;lda #$23	;behind BG attr
;	sta oam+(0*4)+2	
;
;	;= 206 instruction after hit is as late at dot 242
;	;lda #206 ;x	
;	;lda #173 ;x	
;	;lda #185 ;x	worked okay for separate x/y scroll
;	;lda #118 ;x	end of stream #9 first 2-3 tiles on line 31 glitched
;	;lda #86	  ;x 	here we had 1-2 tiles of glitch
;	;lda #69	  ;x 	removed glitch! writes are mostly before Hblank
;	;BUT!  we have X scroll glitches & it's jumpy in the Y
;	;because the writes are sometimes before/after Hblank start
;	lda #253 ;x 	last opaque pixel in HUD
;	sta oam+(0*4)+3	
;
;	jsr reset_scroll ;init the scroll position to default

	jsr cursor_init

	jsr ppu_update ; ppu_update waits until next NMI, turns rendering on (if not already)
			;uploads OAM, palette, and nametable update to PPU

	;wait for the ESP to reset
	@wait_esp_reset:
		jsr reset_esp
		pha 
		ldx #10
		ldy #17
		jsr display_hex_byte
		pla
		cmp #NN_RESET_VAL
		beq @esp_reset

		;wait till next frame
		jsr ppu_update
		jmp @wait_esp_reset

	@esp_reset:

	;send medium message
	;lda #<(test_msg+1)
	;sta ptr0_lo
	;lda #>(test_msg+1)
	;sta ptr0_hi
	mva_ptr ptr0, test_msg+1 ;pascal style string
	;ldx #15 ;max length
	ldx test_msg ;first byte is length
	;lda #15
	;sta arg0
	jsr send_med_msg

	;check if message has been sent
	@wait_sent:
		lda #NN_MSG_SENT
		jsr send_cmd_get_reply
		cmp #0
		bne @wait_sent
	;number of outgoing messages in A
	;wait till zero

	;change conn0 port
	mva_ptr ptr0, conn0_port+1 ;pascal style string
	;ldx #15 ;max length
	ldx conn0_port ;first byte is length
	lda #$0F	;conn 15 is the ESP for meta data
	jsr send_long_msg

	:;wait for message to send before next
		lda #NN_MSG_SENT
		jsr send_cmd_get_reply
		cmp #0
		bne :-

	;change conn0 IP
	mva_ptr ptr0, conn1_ip+1 ;pascal style string
	;ldx #15 ;max length
	ldx conn1_ip ;first byte is length
	lda #$0F	;conn 15 is the ESP for meta data
	jsr send_long_msg

	:;wait for message to send before next
		lda #NN_MSG_SENT
		jsr send_cmd_get_reply
		cmp #0
		bne :-


	mva_ptr ptr0, long_test_msg1+1
	ldx long_test_msg1
	lda #0 ;connection 0
	jsr send_long_msg

	:;wait for message to send before next
		lda #NN_MSG_SENT
		jsr send_cmd_get_reply
		cmp #0
		bne :-


	;change conn0 IP
	mva_ptr ptr0, conn0_ip+1 ;pascal style string
	;ldx #15 ;max length
	ldx conn0_ip ;first byte is length
	lda #$0F	;conn 15 is the ESP for meta data
	jsr send_long_msg

	:;wait for message to send before next
		lda #NN_MSG_SENT
		jsr send_cmd_get_reply
		cmp #0
		bne :-


	mva_ptr ptr0, long_test_msg2+1
	ldx long_test_msg2
	lda #0 ;connection 0
	jsr send_long_msg


	:;wait for message to send before next
		lda #NN_MSG_SENT
		jsr send_cmd_get_reply
		cmp #0
		bne :-

	;change conn0 IP
	mva_ptr ptr0, conn0_tcp+1 ;pascal style string
	;ldx #15 ;max length
	ldx conn0_tcp ;first byte is length
	lda #$0F	;conn 15 is the ESP for meta data
	jsr send_long_msg

	:;wait for message to send before next
		lda #NN_MSG_SENT
		jsr send_cmd_get_reply
		cmp #0
		bne :-


	mva_ptr ptr0, long_test_msg3+1
	ldx long_test_msg3
	lda #0 ;connection 0
	jsr send_long_msg



	;END OF INIT


;;;;;;;;;;;;;;;;;;;;;;;;
;;  MAIN LOOP
;;;;;;;;;;;;;;;;;;;;;;;;
main_loop:
;@temp = reg7 ;hard defined du
;TODO figure out what's breaking the scope

	;nmi set the scroll to $2000 top left corner for HUD

	;ppu_update is complete
	;we may still be in vblank though

;	;count frames, if on NTSC, skip processing every 6th frame to process the game at 50Hz
;	;lda system_type
;	lda skip_frame ;will be negative for PAL/DENDY, positive for NTSC
;	;TODO shouldn't need 2 separate variables, change encoding to make PAL & DENDY negative numbers
;	;then decide to dec frame if pl/mi
;	bmi @non_ntsc_system
;		;NTSC SYSTEM
;		dec skip_frame
;		bpl @non_ntsc_system
;			;wrap from -1 to 5
;			lda #5
;			sta skip_frame
;		
;	@non_ntsc_system:

	;store last frame's gamepad for edge detection
	lda gamepad
	sta gamepad_last
	lda gamepad2
	sta gamepad2_last

	;read gamepads & determine new presses
	ldy #0	;pad1
	jsr gamepad_poll
	sta gamepad
	eor gamepad_last ;invert bits that were presses last frame
	and gamepad	 ;clear bits that were released
	sta gamepad_new

	;if gamepad data changed, send it to esp
	jsr send_gamepad1

	;ldy #1  ;pad2
	;jsr gamepad_poll
	jsr get_pad2_online

	sta gamepad2
	eor gamepad2_last
	and gamepad2
	sta gamepad2_new

	jsr display_gamepad


	;move cursor and go update variables if changed
	jsr cursor_state
	jsr cursor_update

	;START PEEK
	lda gamepad_new
	and #PAD_START
	beq :+
		jsr peek_row
	:
	;SELECT POKE
	lda gamepad_new
	and #PAD_SELECT
	beq :+
		jsr poke_row
	:
	;SEND MESSAGE
	lda gamepad_new
	and #PAD_A
	beq :+
		mva_ptr ptr0, test_msg+1
		ldx test_msg
		jsr send_long_msg
	:

	;check for ESP messages & print to screen
	lda #NN_MSG_POLL
	jsr send_cmd_get_reply
		pha
		ldx #10
		ldy #18
		jsr display_hex_byte
		pla
	
	beq @no_msg

		;get message
		mva_ptr ptr0, esp_msg_buff
		ldx #15
		jsr esp_get_msg
		
		;print message
		mva_ptr ptr0, esp_msg_buff
		ldx #4
		ldy #19
		jsr ppu_write_string

		;mark message as read
		lda #NN_MARK_READ
		jsr send_cmd_no_reply
	@no_msg:


	;check for ESP messages & print to screen
;	lda #NN_MSG_POLL
;	jsr send_cmd_get_reply
;		pha
;		ldx #10
;		ldy #18
;		jsr display_hex_byte
;		pla


nmi_wait:
	;wait for end of frame
	;draw everything in NMI and finish the frame
	jsr ppu_update

	;keep doing this forever!
	jmp main_loop

