; Subroutine called when the state change to this state
game_states_init:
VECTOR(menu_screen_init)
VECTOR(sample_reset_esp_init)
VECTOR(sample_messages_init)

; Subroutine called each frame
game_states_tick:
VECTOR(menu_screen_tick)
VECTOR(sample_reset_esp_tick)
VECTOR(sample_messages_tick)

GAME_STATE_MENU = 0
GAME_STATE_RESET_ESP = 1
GAME_STATE_SAMPLE_MESSAGES = 2

INITIAL_GAME_STATE = GAME_STATE_MENU

#include "game/nesnet_lib.asm"

; Not in original nesnetlib, but useful
wait_esp_reset:
.(
	jsr reset_esp
	cmp #NN_RESET_VAL
	bne wait_esp_reset
	rts
.)

#include "game/menu.asm"
#include "game/sample_reset_esp.asm"
#include "game/sample_messages.asm"
