;
; Contents of the 31 swappable banks
;
; The fixed bank is handled separately
;


#define CURRENT_BANK_NUMBER $00
#echo
#echo === BANK 00 ===
* = $8000
#include "swapable_banks/chr_data.asm"
#include "swapable_banks/bank_filler.asm"

#define CURRENT_BANK_NUMBER $01
#echo
#echo === BANK 01 ===
* = $8000
#include "swapable_banks/sample_connection.asm"
#include "swapable_banks/menu.asm"
#include "swapable_banks/sample_buffer_drop.asm
#include "swapable_banks/bank_filler.asm"

#define CURRENT_BANK_NUMBER $02
#echo
#echo === BANK 02 ===
* = $8000
#include "swapable_banks/sample_files.asm"
#include "swapable_banks/bank_filler.asm"

#define CURRENT_BANK_NUMBER $03
#echo
#echo === BANK 03 ===
* = $8000
#include "swapable_banks/sample_irq.asm"
#include "swapable_banks/bank_filler.asm"

#define CURRENT_BANK_NUMBER $04
#echo
#echo === BANK 04 ===
* = $8000
#include "swapable_banks/sample_noirq.asm"
#include "swapable_banks/bank_filler.asm"

#define CURRENT_BANK_NUMBER $05
#echo
#echo === BANK 05 ===
* = $8000
#include "swapable_banks/sample_udp.asm"
#include "swapable_banks/bank_filler.asm"

#define CURRENT_BANK_NUMBER $06
#echo
#echo === BANK 06 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $07
#echo
#echo === BANK 07 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $08
#echo
#echo === BANK 08 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $09
#echo
#echo === BANK 09 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $0a
#echo
#echo === BANK 0a ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $0b
#echo
#echo === BANK 0b ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $0c
#echo
#echo === BANK 0c ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $0d
#echo
#echo === BANK 0d ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $0e
#echo
#echo === BANK 0e ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $0f
#echo
#echo === BANK 0f ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $10
#echo
#echo === BANK 10 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $11
#echo
#echo === BANK 11 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $12
#echo
#echo === BANK 12 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $13
#echo
#echo === BANK 13 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $14
#echo
#echo === BANK 14 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $15
#echo
#echo === BANK 15 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $16
#echo
#echo === BANK 16 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $17
#echo
#echo === BANK 17 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $18
#echo
#echo === BANK 18 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $19
#echo
#echo === BANK 19 ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $1a
#echo
#echo === BANK 1a ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $1b
#echo
#echo === BANK 1b ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $1c
#echo
#echo === BANK 1c ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $1d
#echo
#echo === BANK 1d ===
#include "swapable_banks/empty_bank.asm"

#define CURRENT_BANK_NUMBER $1e
#echo
#echo === BANK 1e ===
#include "swapable_banks/empty_bank.asm"
