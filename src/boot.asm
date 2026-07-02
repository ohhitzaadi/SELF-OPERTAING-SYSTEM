[org 0x7c00]
KERNEL_OFFSET equ 0x10000

; Standard FAT12 header jump
jmp short start
nop

; Placeholder bytes for OEM name and BPB (total 59 bytes, offset 3 to 61)
times 59 db 0

start:
    ; Set up segment registers and stack
    xor ax, ax
    mov es, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0xFFFF

    ; Save boot drive number (safely with ds = 0)
    mov [BOOT_DRIVE], dl

; Clear screen
mov ax, 0x03
int 0x10

; Print loading message
mov si, MSG_BOOTING
call print_string

; Load kernel from disk
call load_kernel

; Load floppy raw sectors
call load_floppy

; Switch to Protected Mode
call switch_to_pm

jmp $ ; Hang if it returns

; --- 16-bit Printing Subroutine ---
[bits 16]
print_string:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp .loop
.done:
    popa
    ret

; --- Disk Load Subroutine ---
[bits 16]
load_kernel:
    mov si, MSG_LOAD_KERNEL
    call print_string

    mov ax, 0x1000        ; Segment 0x1000 (physical 0x10000)
    mov es, ax
    mov bx, 0x0000        ; Offset 0
    
    ; Start CHS address: Cylinder 0, Head 0, Sector 2
    mov ch, 0x00    ; Cylinder 0
    mov dh, 0x00    ; Head 0
    mov cl, 0x02    ; Sector 2
    
    mov di, 64      ; Read 64 sectors total
    call disk_read_loop
    ret

load_floppy:
    mov si, MSG_LOAD_FLOPPY
    call print_string

    mov ax, 0x2000        ; Segment 0x2000 (physical 0x20000)
    mov es, ax
    mov bx, 0x0000        ; Offset 0
    
    ; Start CHS address: Cylinder 0, Head 0, Sector 1
    mov ch, 0x00    ; Cylinder 0
    mov dh, 0x00    ; Head 0
    mov cl, 0x01    ; Sector 1
    
    mov di, 288     ; Read 288 sectors total (144 KB)
    call disk_read_loop
    ret

disk_read_loop:
    mov si, 5       ; Retry up to 5 times on failure

.retry_read:
    push cx
    push dx
    push di
    push bx
    push si

    mov ah, 0x02    ; BIOS read sector
    mov al, 1       ; Read 1 sector
    mov dl, [BOOT_DRIVE]
    int 0x13        ; BIOS Interrupt

    pop si
    pop bx
    pop di
    pop dx
    pop cx

    jnc .read_success ; Jump on success

    ; Reset disk system on error
    push ax
    push dx
    mov ah, 0x00    ; Reset controller
    mov dl, [BOOT_DRIVE]
    int 0x13
    pop dx
    pop ax

    dec si          ; Decrement retry count
    jnz .retry_read ; Retry if count > 0
    jmp disk_error  ; If all retries failed, trigger hard halt

.read_success:
    ; Print a dot for each read sector
    push ax
    mov ah, 0x0e
    mov al, '.'
    int 0x10
    pop ax

    add bx, 512     ; Move buffer pointer by 512 bytes
    jnz .no_es_inc
    mov ax, es
    add ax, 0x1000  ; Increment segment register by 64KB (since bx wrapped to 0)
    mov es, ax
.no_es_inc:

    ; Increment CHS sector
    inc cl          ; Next sector
    cmp cl, 19      ; Crossed track boundary (18 sectors per track)?
    jl .continue_loop

    mov cl, 1       ; Reset sector to 1
    inc dh          ; Next head
    cmp dh, 2       ; Crossed cylinder boundary (2 heads)?
    jl .continue_loop

    mov dh, 0       ; Reset head to 0
    inc ch          ; Next cylinder

.continue_loop:
    dec di
    jnz disk_read_loop
    ret

disk_error:
    mov si, MSG_DISK_ERROR
    call print_string
    jmp $

; --- GDT Definition ---
gdt_start:
    ; Null descriptor
    dd 0x0
    dd 0x0

gdt_code:
    ; flat mode: base=0, limit=0xfffff (4GB in 4KB pages)
    ; flags: G=1 (4KB pages), D=1 (32-bit), L=0, AVL=0
    ; access: P=1, DPL=00, S=1, E=1, C=0, R=1, A=0 -> 10011010b = 0x9A
    dw 0xffff
    dw 0x0
    db 0x0
    db 0b10011010
    db 0b11001111
    db 0x0

gdt_data:
    ; flat mode: base=0, limit=0xfffff
    ; access: P=1, DPL=00, S=1, E=0, W=1, A=0 -> 10010010b = 0x92
    dw 0xffff
    dw 0x0
    db 0x0
    db 0b10010010
    db 0b11001111
    db 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; --- Transition to 32-bit Protected Mode ---
[bits 16]
switch_to_pm:
    ; Reset segments to 0 in case BIOS disk operations corrupted them
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; Print 'P' to diagnose entry into PM switch
    push ax
    mov ah, 0x0e
    mov al, 'P'
    int 0x10
    pop ax

    ; Set video mode to 13h (320x200, 256-color graphics)
    mov ax, 0x0013
    int 0x10

    cli                     ; 1. Disable interrupts
    lgdt [gdt_descriptor]   ; 2. Load GDT

    ; 3. Enable A20 gate via Fast A20 (Port 0x92)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 4. Set protection enable bit in CR0
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; 5. Far jump to 32-bit code segment
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    ; 6. Initialize segment registers in 32-bit mode
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 7. Set up stack pointer below bootloader
    mov esp, 0x90000

    ; 8. Jump to kernel entry point (loaded at 0x1000)
    jmp KERNEL_OFFSET

; --- Boot Sector Data ---
BOOT_DRIVE db 0
MSG_BOOTING db "Booting NexusOS from MBR...", 0x0D, 0x0A, 0
MSG_LOAD_KERNEL db "Loading Kernel sectors...", 0x0D, 0x0A, 0
MSG_LOAD_FLOPPY db "Loading Floppy FS sectors...", 0x0D, 0x0A, 0
MSG_DISK_ERROR db "Disk read error! Boot halted.", 0x0D, 0x0A, 0

; Pad to 510 bytes, add boot signature
times 510-($-$$) db 0
dw 0xaa55
