[bits 32]

global _start
_start:
    ; 1. Call SYS_PRINT (eax = 1, ebx = string pointer)
    mov eax, 1
    mov ebx, msg
    int 0x80

    ; 2. Call SYS_YIELD (eax = 2)
    mov eax, 2
    int 0x80

    ; 3. Call SYS_PRINT (eax = 1, ebx = string pointer 2)
    mov eax, 1
    mov ebx, msg2
    int 0x80

    ; 4. Call SYS_EXIT (eax = 3)
    mov eax, 3
    int 0x80

section .data
msg db "Hello from Ring 3 User Mode via syscall!", 0x0A, 0
msg2 db "User process yielding and returning success!", 0x0A, 0
