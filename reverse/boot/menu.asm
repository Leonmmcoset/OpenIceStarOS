; Recovered second stage, loaded at 1a00:0000 by the MBR.
; Offsets are relative to that segment. Numeric branch destinations preserve
; the original layout; `make verify` checks every byte against the image.
bits 16
org 0

    dd 0x44b
    dw 0x1d0, 0x1a00, 0
    db 'Please select a system startup'
    times 0x60-($-$$) db ' '
    db '1.IceStarOS'
    times 0x88-($-$$) db ' '
    db '2.IceStarOS(1024*768 8bit)'
    times 0xb0-($-$$) db ' '
    db '3.Reboot Computer'
    times 0xd8-($-$$) db ' '
    db '4.off Computer'
    times 0x122-($-$$) db ' '
    db 'Choose:[1]'
    times 0x14a-($-$$) db ' '
    db ':', 0, '1', 0, 0
    dw 0, 0, 0x101, 0

; RTC interrupt handler, VGA text output and menu control follow.
    push ax
    push bx
    push cx
    push dx
    push es
    push ds
    mov al,0xa
    or al,0x80
    out 0x70,al
    in al,0x71
    test al,0x80
    jnz short 0x15d
    xor al,al
    or al,0x80
    out 0x70,al
    in al,0x71
    push ax
    mov al,0xc
    out 0x70,al
    in al,0x71
    mov ax,0xb800
    mov es,ax
    mov ax,0x1a00
    mov ds,ax
    cmp byte [0x14e],0xd
    jz short 0x1b1
    mov dl,[0x14a]
    cmp dl,0x30
    jz short 0x1b1
    pop ax
    and al,0xf
    add al,0x30
    cmp al,[0x14b]
    jz short 0x1b1
    mov [0x14b],al
    dec dl
    mov [0x14a],dl
    mov [es:0x484],dl
    mov byte [es:0x485],0x7
    mov al,0x20
    out 0xa0,al
    out 0x20,al
    pop ds
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    iret
    push dx
    mov dl,[si]
    mov [es:di],dl
    mov [es:di+0x1],dh
    inc si
    add di,byte +0x2
    loop 0x1bf
    pop dx
    ret
; Set up the text-mode menu and its RTC countdown interrupt.
    mov eax,[0x6]
    mov ds,ax
    mov ax,0xb800
    mov es,ax
    mov ax,0x600
    mov bx,0x700
    mov cx,0x0
    mov dx,0x184f
    int 0x10
    mov si,0xa
    mov di,0x0
    mov cx,0x28
    mov dh,0x7
    call 0x1be
    mov si,0x5a
    mov di,0x140
    mov cx,0x28
    mov dh,0x17
    call 0x1be
    mov si,0x82
    mov di,0x1e0
    mov cx,0x28
    mov dh,0x7
    call 0x1be
    mov si,0xaa
    mov di,0x280
    mov cx,0x28
    mov dh,0x7
    call 0x1be
    mov si,0xd2
    mov di,0x320
    mov cx,0x28
    mov dh,0x7
    call 0x1be
    mov si,0x122
    mov di,0x460
    mov cx,0x28
    mov dh,0x7
    call 0x1be
    mov ax,0x200
    mov bh,0x0
    mov dh,0x7
    mov dl,0x8
    int 0x10
    mov al,0x70
    mov bl,0x4
    mul bl
    mov bx,ax
    cli
    push es
    mov ax,0x0
    mov es,ax
    mov ax,[es:bx]
    mov [0x14f],ax
    mov ax,[es:bx+0x2]
    mov [0x151],ax
    mov word [es:bx],0x157
    mov [es:bx+0x2],cs
    pop es
    mov al,0xb
    or al,0x80
    out 0x70,al
    mov al,0x12
    out 0x71,al
    mov al,0xc
    out 0x70,al
    in al,0x71
    in al,0xa1
    and al,0xfe
    out 0xa1,al
    sti
    cmp byte [0x14a],0x30
    jz near 0x395
    mov ah,0x1
    int 0x16
    jz short 0x285
    mov ah,0x0
    int 0x16
; Numeric keys and arrow keys select an entry; Enter accepts it.
    cmp al,0x31
    jnz short 0x2ab
    mov dl,[0x14c]
    mov [0x14d],dl
    mov byte [0x14c],0x31
    jmp short 0x2e4
    cmp al,0x32
    jnz short 0x2be
    mov dl,[0x14c]
    mov [0x14d],dl
    mov byte [0x14c],0x32
    jmp short 0x2e4
    cmp al,0x33
    jnz short 0x2d1
    mov dl,[0x14c]
    mov [0x14d],dl
    mov byte [0x14c],0x33
    jmp short 0x2e4
    cmp al,0x34
    jnz short 0x344
    mov dl,[0x14c]
    mov [0x14d],dl
    mov byte [0x14c],0x34
    jmp short 0x2e4
    xor ax,ax
    mov al,[0x14c]
    sub al,0x31
    mov cl,0x28
    mul cl
    add ax,strict word 0x5a
    mov si,ax
    xor ax,ax
    mov al,[0x14c]
    sub al,0x31
    mov cl,0xa0
    mul cl
    add ax,0x140
    mov di,ax
    mov cx,0x28
    mov dh,0x17
    call 0x1be
    xor ax,ax
    mov al,[0x14d]
    sub al,0x31
    mov cl,0x28
    mul cl
    add ax,strict word 0x5a
    mov si,ax
    xor ax,ax
    mov al,[0x14d]
    sub al,0x31
    mov cl,0xa0
    mul cl
    add ax,0x140
    mov di,ax
    mov cx,0x28
    mov dh,0x7
    call 0x1be
    mov al,[0x14c]
    mov [es:0x470],al
    mov byte [es:0x471],0x7
    jmp 0x285
    cmp al,0x0
    jnz short 0x38a
    cmp ah,0x50
    jnz short 0x368
    mov dl,[0x14c]
    mov [0x14d],dl
    inc byte [0x14c]
    cmp byte [0x14c],0x35
    jc short 0x2e4
    mov byte [0x14c],0x31
    jmp 0x2e4
    cmp ah,0x48
    jnz short 0x38a
    mov dl,[0x14c]
    mov [0x14d],dl
    dec byte [0x14c]
    cmp byte [0x14c],0x30
    ja near 0x2e4
    mov byte [0x14c],0x34
    jmp 0x2e4
    cmp al,0xd
    jnz near 0x285
    mov byte [0x14e],0xd
    mov al,0x70
    mov bl,0x4
    mul bl
    mov bx,ax
    cli
    push es
    mov ax,0x0
    mov es,ax
    mov ax,[0x14f]
    mov [es:bx],ax
    mov ax,[0x151]
    mov [es:bx+0x2],ax
    mov ax,[es:bx+0x2]
    pop es
    sti
    mov ax,0x600
    mov bh,0x7
    mov cx,0x0
    mov dx,0x184f
    int 0x10
    mov ax,0x200
    mov bh,0x0
    mov dx,0x0
    int 0x10
; Dispatch reboot, APM shutdown, or read the next stage from disk.
    cmp byte [0x14c],0x31
    jz short 0x40e
    cmp byte [0x14c],0x32
    mov word [0x153],0x105
    jz short 0x40e
    cmp byte [0x14c],0x33
    jz short 0x407
    mov ax,0x5301
    xor bx,bx
    int 0x15
    mov ax,0x530e
    xor bx,bx
    mov cx,0x102
    int 0x15
    mov ax,0x5307
    mov bx,0x1
    mov cx,0x3
    int 0x15
    jmp short 0x405
    jmp 0xffff:0x0
    jmp short 0x40c
    mov ax,0x0
    mov ds,ax
    mov dl,[0x7c02]
    mov ax,0x1a80
    mov es,ax
    mov bx,0x0
    mov al,0x4
    mov ch,0x0
    mov cl,0x6
    mov dh,0x0
    mov ah,0x2
    int 0x13
    mov ax,0x1a00
    mov ds,ax
    mov dx,[0x153]
    mov ax,0x1a80
    mov ds,ax
    cmp dx,0x105
    jnz short 0x443
    mov [0xa],dx
    jmp far [0x4]
    jmp short 0x447

    dw 0xaa55
    times 1536-($-$$) db 0
