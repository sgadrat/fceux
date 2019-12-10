;
; Run-length compression for zeros
;  ZIPNT_ZEROS(n) - output n zeros (0 < n < 256)
;  ZIPZ - output one zero
;  ZIPNT_END - end of compressed sequence
;

#define ZIPNT_ZEROS(n) $00, n
#define ZIPZ $00, $01
#define ZIPNT_END $00, $00

; VECTOR(lbl) - Place data representing label's address in little endian
#define VECTOR(lbl) .word lbl
