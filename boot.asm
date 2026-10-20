BITS 16
ORG 0x7C00

%define STAGE2_SECTORS 64

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    sti
    cld

    ;
    ; BIOSがDLに渡した起動ドライブ番号を保存
    ;
    mov [boot_drive], dl

    ;
    ; INT 13h Extensions対応確認
    ;
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]

    int 0x13

    jc disk_error

    cmp bx, 0xAA55
    jne disk_error

    test cx, 0x0001
    jz disk_error

    ;
    ; LBA Extended Read
    ;
    ; LBA 1からSTAGE2_SECTORSセクタを
    ; 0000:8000へロード
    ;
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]

    int 0x13

    jc disk_error

    ;
    ; stage2へジャンプ
    ;
    jmp 0x0000:0x8000


disk_error:
    mov si, msg_error

.print:
    lodsb

    or al, al
    jz hang

    mov ah, 0x0E
    int 0x10

    jmp .print


hang:
    cli
    hlt
    jmp hang


boot_drive:
    db 0


;
; Disk Address Packet
;
; LBA 1からSTAGE2_SECTORSセクタ
; 0000:8000へロード
;
dap:
    db 0x10
    db 0x00

    dw STAGE2_SECTORS

    dw 0x8000
    dw 0x0000

    dd 0x00000001
    dd 0x00000000


msg_error:
    db 'Disk read error', 13, 10, 0


times 510-($-$$) db 0

dw 0xAA55


