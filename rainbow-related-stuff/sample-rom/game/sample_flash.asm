sample_flash_screen_init:
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
.byt ZIPNT_ZEROS(32*6)
.byt ZIPNT_ZEROS(32*5)
.byt $02, $02, $02, $02, $13, $15, $08, $16, $16, $02, $04, $11, $1c, $02, $0e, $08, $1c, $02, $02, $02, $02, $02, $02, $02, $02, $02, $02, $02, $02, $02, $02, $02
.byt $02, $02, $02, $02, $17, $12, $02, $07, $12, $1a, $11, $0f, $12, $04, $07, $02, $04, $02, $0a, $04, $10, $08, $02, $02, $02, $02, $02, $02, $02, $02, $02, $02
;    ------------------- ------------------- ------------------- ------------------- ------------------- ------------------- ------------------- -------------------
.byt ZIPNT_ZEROS(32*7)
.byt ZIPNT_ZEROS(32*6)
nametable_attributes:
.byt ZIPNT_ZEROS(8*8)
.byt ZIPNT_END

.)

sample_flash_screen_tick:
.(
	; Reset drawn nametable buffers
	jsr reset_nt_buffers

	; When player presses a button, modify data in PRG
	lda controller_a_last_frame_btns
	bne end

	lda controller_a_btns
	beq end

	jsr sample_flash_flash_chr
	jsr sample_flash_flash_prg

	end:
	rts
.)

sample_flash_flash_chr:
.(
	cur_addr_lsb=tmpfield1
	cur_addr_msb=tmpfield2

	.(
		; Send upgrade request to server
		lda #11   ; Message length
		sta $5000 ;

		lda #TOESP_MSG_SERVER_SEND_MESSAGE    ; Message type - message for the server
		sta $5000                             ;

		lda #$75  ;
		sta $5000 ;
		lda #$70  ;
		sta $5000 ;
		lda #$67  ;
		sta $5000 ;
		lda #$72  ;
		sta $5000 ;
		lda #$61  ;
		sta $5000 ; "upgradechr"
		lda #$64  ;
		sta $5000 ;
		lda #$65  ;
		sta $5000 ;
		lda #$63  ;
		sta $5000 ;
		lda #$68  ;
		sta $5000 ;
		lda #$72  ;
		sta $5000 ;

		; Wait for server's answer
		wait_server:
			bit $5001
			bpl wait_server

		; Disable rendering
		lda #$00
		sta PPUCTRL
		sta PPUMASK
		sta ppuctrl_val

		; Black screen
		bit PPUSTATUS
		lda #$3f
		sta PPUADDR
		lda #$00
		sta PPUADDR
		lda #$0d
		sta PPUDATA

		; Upgrade prg from server's answer
		jsr upgrade_chr_routine

		rts
	.)

	upgrade_chr_routine:
	.(
		copy_counter=tmpfield3

		; Erase all sectors
		lda #$00
		sta cur_addr_lsb
		lda #$00
		sta cur_addr_msb
		jsr chr_erase
		lda #$08
		sta cur_addr_msb
		jsr chr_erase
		lda #$10
		sta cur_addr_msb
		jsr chr_erase
		lda #$18
		sta cur_addr_msb
		jsr chr_erase

		; Place write vector to the begining of CHR-ROM
		lda #$00
		sta cur_addr_lsb
		lda #$00
		sta cur_addr_msb

		process_one_msg:
			bit $5001
			bpl end

			; Burn ESP header
			lda $5000 ; Gabage byte
			lda $5000 ; message length
			sta copy_counter
			lda $5000 ; message type
			dec copy_counter

			; Write PRG received from server
			ldy #0
			copy_one_byte:
				; Write byte in PRG
				bit PPUSTATUS
				lda #$15
				sta PPUADDR
				lda #$55
				sta PPUADDR
				lda #$aa
				sta PPUDATA

				lda #$0a
				sta PPUADDR
				lda #$aa
				sta PPUADDR
				lda #$55
				sta PPUDATA

				lda #$15
				sta PPUADDR
				lda #$55
				sta PPUADDR
				lda #$A0
				sta PPUDATA

				lda cur_addr_msb
				sta PPUADDR
				lda cur_addr_lsb
				sta PPUADDR
				lda $5000
				sta PPUDATA

				; Increment write vector
				clc
				lda #1
				adc cur_addr_lsb
				sta cur_addr_lsb
				lda #0
				adc cur_addr_msb
				sta cur_addr_msb

				; Loop until message's end
				dec copy_counter
				bne copy_one_byte
			end_copy:

			; Inconditional loop (we cannot use jmp, this subroutine will be relocated, we need relative addressing)
			lda #0
			beq process_one_msg
		end:

		rts
	.)

	chr_erase:
	.(
		bit PPUSTATUS
		lda #$15
		sta PPUADDR
		lda #$55
		sta PPUADDR
		lda #$aa
		sta PPUDATA

		lda #$0a
		sta PPUADDR
		lda #$aa
		sta PPUADDR
		lda #$55
		sta PPUDATA

		lda #$15
		sta PPUADDR
		lda #$55
		sta PPUADDR
		lda #$80
		sta PPUDATA

		lda #$15
		sta PPUADDR
		lda #$55
		sta PPUADDR
		lda #$aa
		sta PPUDATA

		lda #$0a
		sta PPUADDR
		lda #$aa
		sta PPUADDR
		lda #$55
		sta PPUDATA

		lda cur_addr_msb
		sta PPUADDR
		lda cur_addr_lsb
		sta PPUADDR
		lda #$30
		sta PPUDATA

		rts
	.)
.)

sample_flash_flash_prg:
.(
	memory_routine = $0400

	.(
		; Send upgrade request to server
		lda #11   ; Message length
		sta $5000 ;

		lda #TOESP_MSG_SERVER_SEND_MESSAGE    ; Message type - message for the server
		sta $5000                             ;

		lda #$75  ;
		sta $5000 ;
		lda #$70  ;
		sta $5000 ;
		lda #$67  ;
		sta $5000 ;
		lda #$72  ;
		sta $5000 ;
		lda #$61  ;
		sta $5000 ; "upgradeprg"
		lda #$64  ;
		sta $5000 ;
		lda #$65  ;
		sta $5000 ;
		lda #$70  ;
		sta $5000 ;
		lda #$72  ;
		sta $5000 ;
		lda #$67  ;
		sta $5000 ;

		; Copy upgrade routine in RAM
		ldx #upgrade_routine_end-upgrade_routine
		copy_one_byte:
			dex
			cpx #$ff
			beq end_copy

			lda upgrade_routine, x
			sta memory_routine, x

			jmp copy_one_byte
		end_copy:

		; Wait for server's answer
		wait_server:
			bit $5001
			bpl wait_server

		; Disable rendering
		lda #$00
		sta PPUCTRL
		sta PPUMASK
		sta ppuctrl_val

		; Upgrade prg from server's answer
		jsr memory_routine
	.)

	upgrade_routine:
	.(
		cur_addr_lsb=tmpfield1
		cur_addr_msb=tmpfield2
		copy_counter=tmpfield3

		; Erase all sectors
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $8000
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $9000
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $a000
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $b000
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $c000
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $d000
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $e000
		jsr memory_routine+(unlock_erase_mode-upgrade_routine)
		sta $f000

		; Place write vector to the begining of PRG
		lda #$00
		sta cur_addr_lsb
		lda #$80
		sta cur_addr_msb

		process_one_msg:
			bit $5001
			bpl end

			; Burn ESP header
			lda $5000 ; Gabage byte
			lda $5000 ; message length
			sta copy_counter
			lda $5000 ; message type
			dec copy_counter

			; Write PRG received from server
			ldy #0
			copy_one_byte:
				; Write byte in PRG
				lda #$aa
				sta $d555
				lda #$55
				sta $aaaa
				lda #$a0
				sta $d555

				lda $5000
				sta (cur_addr_lsb), y

				; Increment write vector
				clc
				lda #1
				adc cur_addr_lsb
				sta cur_addr_lsb
				lda #0
				adc cur_addr_msb
				sta cur_addr_msb

				; Loop until message's end
				dec copy_counter
				bne copy_one_byte
			end_copy:

			; Inconditional loop (we cannot use jmp, this subroutine will be relocated, we need relative addressing)
			lda #0
			beq process_one_msg
		end:

		; Jump to entry point
		ldx #$ff
		txs
		jmp ($fffc)
	.)

	unlock_erase_mode:
	.(
		lda #$aa
		ldx #$55
		ldy #$80
		sta $d555
		stx $aaaa
		sty $d555
		sta $d555
		stx $aaaa
		lda #$30
		rts
	.)
	upgrade_routine_end:
#print upgrade_routine_end-upgrade_routine
.)
