; Subroutine called when the state change to this state
game_states_init:
VECTOR(main_screen_init)

; Subroutine called each frame
game_states_tick:
VECTOR(main_screen_tick)

#define GAME_STATE_MAIN 0

#include "game/main_screen.asm"
