; Building the project
;  xa sample512.asm -C -o sample512.nes
;
; You can force declaration of the rom as mapper 30 (instead of 3870) with
;  xa sample512.asm -C -DUSE_MAPPER_30 -o sample512.nes
;

; iNES header

#include "others/ines_header.asm"

; No-data declarations

#include "others/nes_labels.asm"
#include "others/mem_labels.asm"
#include "others/constants.asm"
#include "others/macros.asm"
#include "others/rainbow_lib_macros.asm"

; PRG-ROM

#include "swapable_banks/swapable_banks.asm"

#echo
#echo ===== FIXED-BANK =====
* = $c000 ; $c000 is where the PRG fixed bank rom is mapped in CPU space, so code position is relative to it
#include "fixed_bank/fixed_bank.asm"
#endif
