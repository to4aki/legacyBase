BITS 16

SECTION .text

GLOBAL stage2_start
EXTERN app_main


stage2_start:
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax

    ;
    ; VGA Mode 13h
    ; 320 x 200
    ; 256 colors
    ;
    mov ax, 0x0013
    int 0x10

    ;
    ; Protected Modeへ移行
    ;
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    jmp CODE_SEG:protected_mode


gdt_start:

gdt_null:
    dq 0x0000000000000000


gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00


gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00


gdt_end:


gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start


BITS 32

protected_mode:
    mov ax, DATA_SEG

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000

    call app_main


hang:
    cli
    hlt
    jmp hang


