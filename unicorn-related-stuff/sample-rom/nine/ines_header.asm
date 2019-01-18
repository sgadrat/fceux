.asc "NES", $1A ; iNES magic
.byt 2          ; PRG section occupies 2*16KiB memory
.byt 1          ; CHR section occupies 1* 8KiB memory
.byt %00000000  ; Flags 6 - mapper 0, horizontal mirroring, no trainer, no persistent memory
.byt %00001000  ; Flags 7 - mapper 0, NES 2.0, not PlayChoice10, not VS unisystem
.byt %00001111  ;
.byt %00000000  ; Flags 9
.byt 0          ;
.byt 0          ; 11
.byt %00000001  ;
.byt 0          ; 13
.byt 0          ;
.byt 0          ; 15
