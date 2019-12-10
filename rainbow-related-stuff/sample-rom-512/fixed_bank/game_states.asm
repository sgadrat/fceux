; Subroutine called when the state change to this state
game_states_init:
VECTOR(sample_noirq_screen_init)
VECTOR(sample_irq_screen_init)
VECTOR(sample_files_init)
VECTOR(sample_connection_screen_init)

; Subroutine called each frame
game_states_tick:
VECTOR(sample_noirq_screen_tick)
VECTOR(sample_irq_screen_tick)
VECTOR(sample_files_screen_tick)
VECTOR(sample_connection_screen_tick)

game_states_bank:
.byt SAMPLE_NOIRQ_BANK
.byt SAMPLE_IRQ_BANK
.byt SAMPLE_FILES_BANK
.byt SAMPLE_CONNECTION_BANK

#define GAME_STATE_NOIRQ_SAMPLE 0
#define GAME_STATE_IRQ_SAMPLE 1
#define GAME_STATE_FILES_SAMPLE 2
#define GAME_STATE_CONNECTION_SAMPLE 3

INITIAL_GAME_STATE = GAME_STATE_NOIRQ_SAMPLE
