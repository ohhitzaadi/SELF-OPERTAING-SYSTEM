[bits 32]
global _start
global switch_to_task
extern kmain

_start:
    ; Ensure segment registers are correct
    mov ax, 0x10    ; Data segment selector in GDT (0x10)
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Stack is already set up in bootloader at 0x90000, let's keep it
    call kmain
    
    ; If kernel returns, hang
    cli
.hang:
    hlt
    jmp .hang

; Cooperative Context Switch
; switch_to_task(uint32_t *old_esp, uint32_t new_esp)
switch_to_task:
    ; 1. Save registers of the current task (callee-saved)
    push ebp
    push ebx
    push esi
    push edi

    ; 2. Save current ESP to old_esp
    ; Stack offset shift:
    ;   [esp + 0]  = edi
    ;   [esp + 4]  = esi
    ;   [esp + 8]  = ebx
    ;   [esp + 12] = ebp
    ;   [esp + 16] = return EIP
    ;   [esp + 20] = old_esp (argument 1)
    ;   [esp + 24] = new_esp (argument 2)
    mov eax, [esp + 20]     ; eax = pointer to old_esp
    mov [eax], esp          ; *old_esp = esp

    ; 3. Load the new stack pointer (new_esp)
    mov esp, [esp + 24]     ; esp = new_esp

    ; 4. Restore registers for the new task
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; 5. Return to the new task's return EIP
    ret

global load_idt
load_idt:
    mov edx, [esp + 4]
    lidt [edx]
    ret

global isr_timer
extern timer_handler
isr_timer:
    pusha
    call timer_handler
    popa
    iretd

global isr_keyboard
extern keyboard_handler
isr_keyboard:
    pusha
    call keyboard_handler
    popa
    iretd

global isr_mouse
extern mouse_handler
isr_mouse:
    pusha
    call mouse_handler
    popa
    iretd

global enter_user_mode
enter_user_mode:
    ; Argument 1: [esp + 4] = entry_point (EIP)
    ; Argument 2: [esp + 8] = user_stack (ESP)
    mov edx, [esp + 4]      ; edx = entry_point
    mov ecx, [esp + 8]      ; ecx = user_stack

    cli

    ; Setup segment selectors for User Data
    mov ax, 0x23            ; User Data selector (index 4 * 8 = 0x20 | RPL 3 = 0x23)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push Stack parameters for iret
    push eax                ; User SS
    push ecx                ; User ESP
    
    pushfd
    pop eax
    or eax, 0x200           ; Set IF in EFLAGS to enable user-mode interrupts
    push eax                ; EFLAGS
    
    mov ax, 0x1B            ; User Code selector (index 3 * 8 = 0x18 | RPL 3 = 0x1B)
    push eax                ; User CS
    push edx                ; User EIP

    iretd

global isr_syscall
extern syscall_handler
isr_syscall:
    cli
    pusha                   ; Save user context
    
    mov ax, 0x10            ; Switch ds/es to Kernel Data Segment
    mov ds, ax
    mov es, ax
    
    push esp                ; Pass registers pointer to C handler
    call syscall_handler
    add esp, 4
    
    mov ax, 0x23            ; Switch ds/es back to User Data Segment
    mov ds, ax
    mov es, ax
    
    popa                    ; Restore user context
    iretd
