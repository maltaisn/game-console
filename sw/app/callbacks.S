
; The callback table
.section .callback_vectors
.global  __callback_vectors
__callback_vectors:
    jmp callback_loop
    jmp callback_draw
    jmp callback_sleep
    jmp callback_wakeup
    jmp callback_sleep_scheduled
    jmp __vector_uart_dre
    jmp __vector_uart_rxc

; Setup callback (must be implemented)

.section .text.callback_setup
.weak callback_setup
callback_setup:
    ; nothing

; Weak no-op implementation of the callback handlers.

.section .text.callback_loop
.weak callback_loop
callback_loop:
    ldi r24, 0  ; return false;
    ret

.section .text.callback_draw
.weak callback_draw
.set callback_draw, __callback_void_default

.section .text.callback_sleep
.weak callback_sleep
.set callback_sleep, __callback_void_default

.section .text.callback_wakeup
.weak callback_wakeup
.set callback_wakeup, __callback_void_default

.section .text.callback_sleep_scheduled
.weak callback_sleep_scheduled
.set callback_sleep_scheduled, __callback_void_default

.section .text.__vector_uart_dre
.weak __vector_uart_dre
.set __vector_uart_dre, __callback_interrupt_default

.section .text.__vector_uart_rxc
.weak __vector_uart_rxc
.set __vector_uart_rxc, __callback_interrupt_default

__callback_void_default:
    ret

__callback_interrupt_default:
    reti
