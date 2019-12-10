;
; Labels reset between states $00-$cf
;

;
; Global labels $d0-$ef
;

controller_a_btns = $e0
controller_b_btns = $e1
controller_a_last_frame_btns = $e2
controller_b_last_frame_btns = $e3
global_game_state = $e4

; State of the NMI processing
;  $00 - NMI processed
;  $01 - Waiting for the next NMI to be processed
nmi_processing = $e5

scroll_x = $e6
scroll_y = $e7
ppuctrl_val = $e8

;
; Memory registers $f0-$ff
;

tmpfield1 = $f0
tmpfield2 = $f1
tmpfield3 = $f2
tmpfield4 = $f3
tmpfield5 = $f4
tmpfield6 = $f5
tmpfield7 = $f6
tmpfield8 = $f7
tmpfield9 = $f8
tmpfield10 = $f9
tmpfield11 = $fa
tmpfield12 = $fb
tmpfield13 = $fc
tmpfield14 = $fd
tmpfield15 = $fe
tmpfield16 = $ff

;
; Non zero-page labels
;

stack = $0100
oam_mirror = $0200
nametable_buffers = $0300
previous_global_game_state = $540
virtual_frame_cnt = $0700
skip_frames_to_50hz = $0701
