; Recovered from sector 0 of IceStarOS_x86_32M.vhd.
; The BIOS loads this sector at 0000:7c00 and supplies the drive in DL.
bits 16
org 0x7c00

    jmp short start
boot_drive: db 0

start:
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov ax, 0x7c00
    mov sp, ax
    mov [boot_drive], dl

    ; Read CHS 0/0/2 through 0/0/4 into 1a00:0000.
    mov ax, 0x1a00
    mov es, ax
    mov bx, 0
    mov al, 3
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov ah, 2
    int 0x13

    mov ax, 0x1a00
    mov ds, ax
    jmp far [0x0004]
    jmp short $

    times 446-($-$$) db 0
    ; Original active FAT16 partition: start LBA 17, length 65467 sectors.
    db 0x80, 0x01, 0x01, 0x00, 0x04, 0x03, 0xd1, 0xc2
    dd 17, 65467
    times 48 db 0
    dw 0xaa55
