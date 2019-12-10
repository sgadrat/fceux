.asc "NES", $1A ; iNES magic
.byt 32         ; PRG section occupies 32*16KiB memory
.byt 0          ; CHR-ROM section occupies 0*8KiB memory (we use CHR-RAM)
.byt %11100010  ; Flags 6 NNNN FTBM - mapper lower nibble = 0xE, no four screen, no trainer, persistent memory, horizontal mirroring
.byt %00011000  ; Flags 7 NNNN 10TT - mapper midle nibble = 0x1, NES 2.0, NES/Famicom
#ifndef USE_MAPPER_30
.byt %00001111  ; Flags 8 SSSS NNNN - submapper 0, mapper upper nibble 0xF (mapper 0xF1E = 3870)
#else
.byt %00000000  ; Flags 8 SSSS NNNN - submapper 0, mapper upper nibble = 0x0 (mapper 0x01E = 30)
#endif
.byt %00000000  ; Flags 9 CCCC PPPP - CHR-ROM size MSB = 0, PRG-ROM size MSB = 0
.byt 0          ; Flags 10
.byt %00000111  ; Flags 11 cccc CCCC - CHR-NVRAM = 0 bytes, CHR-RAM = 64<<7 = 8192 bytes
.byt %00000001  ; Flags 12 .... ..VV - PAL timing
.byt 0          ; Flags 13
.byt 0          ; Flags 14
.byt 0          ; Flags 15
