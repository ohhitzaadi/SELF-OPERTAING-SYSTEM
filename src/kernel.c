#include <stddef.h>
#include <stdint.h>
#include "kernel/mem.h"
#include "kernel/sched.h"
#include "kernel/drivers/io.h"
#include "kernel/drivers/ata.h"
#include "kernel/drivers/mouse.h"
#include "kernel/drivers/timer.h"
#include "services/window_manager.h"
#include "services/file_service.h"

#define COLOR_WHITE_ON_BLACK 0x0F
#define COLOR_GREEN_ON_BLACK 0x0A
#define COLOR_RED_ON_BLACK 0x0C
#define VGA_ADDRESS 0xB8000

// Console variables
int cursor_x = 0;
unsigned char current_color = COLOR_WHITE_ON_BLACK;
char console_history[150][53] = {{0}};
unsigned char console_color_history[150][53] = {{0}};
int history_line_count = 1;
int scroll_offset = 0;

void gfx_redraw_console() {
  wm_clear(0);

  int end_line = history_line_count - 1 - scroll_offset;
  int start_line = end_line - 23;
  if (start_line < 0)
    start_line = 0;

  int screen_row = 0;
  for (int i = start_line; i <= end_line && i < history_line_count; i++) {
    for (int col = 0; col < 53; col++) {
      char c = console_history[i][col];
      unsigned char col_color = console_color_history[i][col];
      if (c != '\0' && c != ' ') {
        wm_draw_char(col * 6, screen_row * 8, c, col_color);
      }
    }
    screen_row++;
  }

  if (scroll_offset == 0) {
    int cur_row = screen_row - 1;
    if (cur_row >= 0 && cur_row < 24) {
      wm_draw_char(cursor_x * 6, cur_row * 8, '_', COLOR_WHITE_ON_BLACK);
    }
  } else {
    wm_draw_string(70, 192, "-- SCROLLBACK (Esc to exit) --", 14);
  }

  wm_swap();
}

void kprint_char_color(char c, unsigned char color) {
  if (c == '\n') {
    cursor_x = 0;
    history_line_count++;
    if (history_line_count > 150) {
      for (int i = 0; i < 149; i++) {
        for (int j = 0; j < 53; j++) {
          console_history[i][j] = console_history[i + 1][j];
          console_color_history[i][j] = console_color_history[i + 1][j];
        }
      }
      history_line_count = 150;
    }
    for (int j = 0; j < 53; j++) {
      console_history[history_line_count - 1][j] = '\0';
      console_color_history[history_line_count - 1][j] = 0;
    }
  } else if (c == '\r') {
    cursor_x = 0;
  } else if (c == '\b') {
    if (cursor_x > 0) {
      cursor_x--;
      console_history[history_line_count - 1][cursor_x] = '\0';
      console_color_history[history_line_count - 1][cursor_x] = 0;
    }
  } else {
    console_history[history_line_count - 1][cursor_x] = c;
    console_color_history[history_line_count - 1][cursor_x] = color;
    cursor_x++;
    if (cursor_x >= 53) {
      cursor_x = 0;
      history_line_count++;
      if (history_line_count > 150) {
        for (int i = 0; i < 149; i++) {
          for (int j = 0; j < 53; j++) {
            console_history[i][j] = console_history[i + 1][j];
            console_color_history[i][j] = console_color_history[i + 1][j];
          }
        }
        history_line_count = 150;
      }
      for (int j = 0; j < 53; j++) {
        console_history[history_line_count - 1][j] = '\0';
        console_color_history[history_line_count - 1][j] = 0;
      }
    }
  }

  scroll_offset = 0;
  gfx_redraw_console();
}

void kprint_char(char c) { kprint_char_color(c, current_color); }

void kprint(const char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    kprint_char(str[i]);
  }
}

void kprint_color(const char *str, unsigned char color) {
  unsigned char old = current_color;
  current_color = color;
  kprint(str);
  current_color = old;
}

void kprint_int(int n) {
  char buf[32];
  itoa(n, buf);
  kprint(buf);
}

void clear_screen() {
  history_line_count = 1;
  for (int j = 0; j < 53; j++) {
    console_history[0][j] = '\0';
    console_color_history[0][j] = 0;
  }
  cursor_x = 0;
  scroll_offset = 0;
  gfx_redraw_console();
}

// CPU configurations: IDT & GDT
struct idt_entry {
  unsigned short base_lo;
  unsigned short sel;
  unsigned char always0;
  unsigned char flags;
  unsigned short base_hi;
} __attribute__((packed));

struct idt_ptr {
  unsigned short limit;
  unsigned int base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void load_idt(unsigned int idt_ptr_addr);
extern void isr_timer();
extern void isr_keyboard();
extern void isr_mouse();

void idt_set_gate(unsigned char num, uintptr_t base, unsigned short sel,
                  unsigned char flags) {
  idt[num].base_lo = base & 0xFFFF;
  idt[num].base_hi = (base >> 16) & 0xFFFF;
  idt[num].sel = sel;
  idt[num].always0 = 0;
  idt[num].flags = flags;
}

void pic_remap() {
  outb(0x20, 0x11);
  outb(0xA0, 0x11);
  outb(0x21, 0x20);
  outb(0xA1, 0x28);
  outb(0x21, 0x04);
  outb(0xA1, 0x02);
  outb(0x21, 0x01);
  outb(0xA1, 0x01);
  outb(0x21, 0xF8); // Enable IRQ0 (Timer), IRQ1 (Keyboard), and IRQ2 (Cascade)
  outb(0xA1, 0xEF); // Enable IRQ12 (Mouse)
}

void idt_init() {
  idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
  idtp.base = (uintptr_t)&idt;
  memset(&idt, 0, sizeof(idt));

  idt_set_gate(0x20, (uintptr_t)isr_timer, 0x08, 0x8E);
  idt_set_gate(0x21, (uintptr_t)isr_keyboard, 0x08, 0x8E);
  idt_set_gate(0x2C, (uintptr_t)isr_mouse, 0x08, 0x8E);

  void isr_syscall();
  idt_set_gate(0x80, (uintptr_t)isr_syscall, 0x08,
               0xEE); // DPL = 3 for User System Calls

  load_idt((uintptr_t)&idtp);
}

void enable_interrupts() { __asm__ volatile("sti"); }

// GDT & TSS Structs
struct gdt_entry {
  unsigned short limit_low;
  unsigned short base_low;
  unsigned char base_middle;
  unsigned char access;
  unsigned char granularity;
  unsigned char base_high;
} __attribute__((packed));

struct gdt_ptr {
  unsigned short limit;
  unsigned int base;
} __attribute__((packed));

struct tss_entry {
  unsigned int link;
  unsigned int esp0;
  unsigned int ss0;
  unsigned int esp1;
  unsigned int ss1;
  unsigned int esp2;
  unsigned int ss2;
  unsigned int cr3;
  unsigned int eip;
  unsigned int eflags;
  unsigned int eax;
  unsigned int ecx;
  unsigned int edx;
  unsigned int ebx;
  unsigned int esp;
  unsigned int ebp;
  unsigned int esi;
  unsigned int edi;
  unsigned int es;
  unsigned int cs;
  unsigned int ss;
  unsigned int ds;
  unsigned int fs;
  unsigned int gs;
  unsigned int ldt;
  unsigned short trap;
  unsigned short iomap_base;
} __attribute__((packed));

struct gdt_entry gdt[6];
struct gdt_ptr gp;
struct tss_entry tss;

void gdt_set_gate(int num, unsigned long base, unsigned long limit,
                  unsigned char access, unsigned char gran) {
  gdt[num].base_low = (base & 0xFFFF);
  gdt[num].base_middle = (base >> 16) & 0xFF;
  gdt[num].base_high = (base >> 24) & 0xFF;
  gdt[num].limit_low = (limit & 0xFFFF);
  gdt[num].granularity = ((limit >> 16) & 0x0F);
  gdt[num].granularity |= (gran & 0xF0);
  gdt[num].access = access;
}

void gdt_init() {
  gp.limit = (sizeof(struct gdt_entry) * 6) - 1;
  gp.base = (uintptr_t)&gdt;

  gdt_set_gate(0, 0, 0, 0, 0);
  gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
  gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
  gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
  gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

  memset(&tss, 0, sizeof(tss));
  tss.ss0 = 0x10;
  tss.esp0 = 0x90000;

  uintptr_t tss_base = (uintptr_t)&tss;
  uintptr_t tss_limit = sizeof(tss) - 1;
  gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x40);

  __asm__ volatile("lgdt %0" : : "m"(gp));

  __asm__ volatile("ljmp $0x08, $1f\n"
                   "1:\n"
                   "mov $0x10, %%ax\n"
                   "mov %%ax, %%ds\n"
                   "mov %%ax, %%es\n"
                   "mov %%ax, %%fs\n"
                   "mov %%ax, %%gs\n"
                   "mov %%ax, %%ss\n"
                   :
                   :
                   : "ax");

  __asm__ volatile("ltr %%ax" : : "a"(0x28));
}

// User execution trampoline & spawn helper
extern void enter_user_mode(uintptr_t entry_point, uintptr_t user_stack);
extern void user_mode_trampoline();

void kmain() {
  clear_screen();
  kprint_color("========================================================\n",
               COLOR_GREEN_ON_BLACK);
  kprint_color("              NexusOS Modular Kernel                    \n",
               COLOR_GREEN_ON_BLACK);
  kprint_color("========================================================\n",
               COLOR_GREEN_ON_BLACK);
  kprint("Booted into 32-bit Protected Mode successfully.\n");
  kprint("Initializing kernel services...\n");

  // Initialize physical memory
  pmm_init();
  kprint("[MEM] Physical Page Allocator initialized (32MB RAM).\n");

  // Initialize virtual memory
  vmm_init();

  // Initialize CPU tables
  gdt_init();
  idt_init();
  pic_remap();
  timer_init(100);

  // Initialize mouse (while interrupts are disabled to prevent the ISR from swallowing config ACKs)
  mouse_init();
  kprint("[DEV] PS/2 Mouse initialized successfully.\n");

  enable_interrupts();
  kprint("[CPU] IDT, PIC remapped, PIT timer configured at 100Hz. Interrupts enabled.\n");

  // Initialize ATA hard drive
  ata_init();

  // Initialize ram-disk file system
  fs_init();
  kprint("[FS] Memory File System (RAM Disk) initialized.\n");

  // Initialize multitasking scheduler
  sched_init();
  kprint("[SCHED] Task Process Control Table initialized.\n");

  // Spawn Ring 3 Shell (SHELL.ELF)
  fat_dirent_t *shell_entry = fat_find_file("SHELL.ELF");
  if (!shell_entry) {
    kprint("[PANIC] SHELL.ELF not found in FAT filesystem!\n");
    while (1);
  }

  // Load SHELL.ELF from FAT disk to base address
  unsigned char *elf_buf = (unsigned char *)malloc(shell_entry->file_size);
  if (!elf_buf) {
    kprint("[PANIC] Out of memory loading SHELL.ELF!\n");
    while (1);
  }

  fat_read_file("SHELL.ELF", elf_buf);
  unsigned int entry_point = 0;
  // Declare Elf32 structures locally or in headers
  extern int elf_load(const unsigned char *elf_data, unsigned int *entry_point);
  int load_status = elf_load(elf_buf, &entry_point);
  free(elf_buf);

  if (load_status != 0) {
    kprint("[PANIC] Failed to parse SHELL.ELF format!\n");
    while (1);
  }

  // Define Shell as Process 0
  int pid = create_process(user_mode_trampoline, "SHELL.ELF");
  process_table[pid].user_entry = entry_point;
  process_table[pid].user_stack = 0x480000; // Shell stack base
  process_table[pid].state = 1; // READY

  // Set up dummy running process so sys_yield switches to Shell
  current_process_id = 1;
  process_table[1].state = 2; // RUNNING

  kprint("\nNexusOS user-space loader launching SHELL.ELF...\n\n");

  // Yield to start multitasking, which will run the shell process!
  sys_yield();

  // Keep CPU idle in case the scheduler returns
  while (1) {
    __asm__ volatile("hlt");
  }
}
