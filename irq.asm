BITS 32

SECTION .text

GLOBAL irq0_entry
GLOBAL irq1_entry

EXTERN timer_irq_handler
EXTERN keyboard_irq_handler


irq0_entry:
    pushad

    call timer_irq_handler

    popad
    iretd


irq1_entry:
    pushad

    call keyboard_irq_handler

    popad
    iretd

