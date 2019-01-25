; Subroutine called when the state change to this state
game_states_init:
VECTOR(sample_noirq_screen_init)
VECTOR(sample_irq_screen_init)

; Subroutine called each frame
game_states_tick:
VECTOR(sample_noirq_screen_tick)
VECTOR(sample_irq_screen_tick)

#define GAME_STATE_NOIRQ_SAMPLE 0
#define GAME_STATE_IRQ_SAMPLE 0

#include "game/sample_noirq.asm"
#include "game/sample_irq.asm"
