.asc "NES", $1A ; iNES magic
.byt 2          ; PRG section occupies 2*16KiB memory
.byt 1          ; CHR section occupies 1* 8KiB memory
.byt %00010000  ; Flags 6 - mapper 3841, horizontal mirroring, no trainer, no persistent memory
.byt %00001000  ; Flags 7 - mapper 3841, NES 2.0, not PlayChoice10, not VS unisystem
.byt %00001111  ; Flags 8 - submapper 0, mapper 3841
.byt %00000000  ; Flags 9 - CHR-ROM size MSB = 0, PRG-ROM size MSB = 0
.byt 0          ; Flags 10
.byt 0          ; Flags 11
.byt %00000000  ; Flags 12 - CPU timing NTSC
.byt 0          ; Flags 13
.byt 0          ; Flags 14
.byt 0          ; Flags 15
