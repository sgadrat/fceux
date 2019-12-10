;
; Print some PRG-ROM space usage information
;

#echo
#echo FIXED-bank total space:
#print $10000-$c000
#echo
#echo FIXED-bank free space:
#print $fffa-*

;
; Fill code bank and set entry points vectors
;

#if $fffa-* < 0
#error Out of space in fixed bank
#else
.dsb $fffa-*, 0     ;aligning
.word nmi           ;entry point for VBlank interrupt  (NMI)
.word reset         ;entry point for program start     (RESET)
.word cursed        ;entry point for masking interrupt (IRQ)
#endif
