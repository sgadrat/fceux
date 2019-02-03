; Subroutine called when the state change to this state
game_states_init:
VECTOR(sample_noirq_screen_init)
VECTOR(sample_irq_screen_init)
VECTOR(sample_flash_screen_init)
VECTOR(sample_files_init)
VECTOR(sample_connection_screen_init)

; Subroutine called each frame
game_states_tick:
VECTOR(sample_noirq_screen_tick)
VECTOR(sample_irq_screen_tick)
VECTOR(sample_flash_screen_tick)
VECTOR(sample_files_screen_tick)
VECTOR(sample_connection_screen_tick)

#define GAME_STATE_NOIRQ_SAMPLE 0
#define GAME_STATE_IRQ_SAMPLE 1
#define GAME_STATE_FLASH_SAMPLE 2
#define GAME_STATE_FILES_SAMPLE 3
#define GAME_STATE_CONNECTION_SAMPLE 4

#include "game/sample_noirq.asm"
#include "game/sample_irq.asm"
#include "game/sample_flash.asm"
#include "game/sample_files.asm"
#include "game/sample_connection.asm"
