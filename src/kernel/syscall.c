#include "mem.h" // IWYU pragma: keep
#include "sched.h"
#include "services/window_manager.h"
#include "services/file_service.h"
#include "services/device_service.h"

typedef struct {
  unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
} registers_t;

// ELF parsing structures
typedef struct {
  unsigned char e_ident[16];
  unsigned short e_type;
  unsigned short e_machine;
  unsigned int e_version;
  unsigned int e_entry;
  unsigned int e_phoff;
  unsigned int e_shoff;
  unsigned int e_flags;
  unsigned short e_ehsize;
  unsigned short e_phentsize;
  unsigned short e_phnum;
  unsigned short e_shentsize;
  unsigned short e_shnum;
  unsigned short e_shstrndx;
} Elf32_Ehdr;

typedef struct {
  unsigned int p_type;
  unsigned int p_offset;
  unsigned int p_vaddr;
  unsigned int p_paddr;
  unsigned int p_filesz;
  unsigned int p_memsz;
  unsigned int p_flags;
  unsigned int p_align;
} Elf32_Phdr;

// Diagnostic log functions
extern void kprint(const char *str);
extern void kprint_int(int n);

// ELF Loader implementation
int elf_load(const unsigned char *elf_data, unsigned int *entry_point) {
  Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_data;
  if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
      ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    return -1;
  }
  if (ehdr->e_ident[4] != 1 || ehdr->e_ident[5] != 1) {
    return -2;
  }
  Elf32_Phdr *phdrs = (Elf32_Phdr *)(elf_data + ehdr->e_phoff);
  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdrs[i].p_type == 1) { // PT_LOAD
      void *dest = (void *)phdrs[i].p_vaddr;
      const void *src = elf_data + phdrs[i].p_offset;
      memcpy(dest, src, phdrs[i].p_filesz);
      if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
        memset((char *)dest + phdrs[i].p_filesz, 0,
               phdrs[i].p_memsz - phdrs[i].p_filesz);
      }
    }
  }
  *entry_point = ehdr->e_entry;
  return 0;
}

// User-space execution trampoline defined in sched.c
extern void user_mode_trampoline();

static int sys_spawn(const char *filename) {
  int file_size = -1;
  int is_fat16 = 0;

  fat_dirent_t *entry = fat_find_file(filename);
  if (entry) {
    file_size = entry->file_size;
  } else {
    file_size = fat16_get_file_size(filename);
    if (file_size >= 0) {
      is_fat16 = 1;
    }
  }

  if (file_size < 0) return -1;

  unsigned char *elf_buf = (unsigned char *)malloc(file_size);
  if (!elf_buf) return -1;

  int bytes_read = 0;
  if (is_fat16) {
    bytes_read = fat16_read_file(filename, elf_buf);
  } else {
    bytes_read = fat_read_file(filename, elf_buf);
  }

  if (bytes_read <= 0) {
    free(elf_buf);
    return -1;
  }

  unsigned int entry_point = 0;
  int load_status = elf_load(elf_buf, &entry_point);
  free(elf_buf);

  if (load_status != 0) {
    return -1;
  }

  int pid = create_process(user_mode_trampoline, filename);
  if (pid == -1) return -1;

  // Allocate stack base (e.g. 0x480000, 0x580000, etc.)
  unsigned int stack_addr = (entry_point & 0xFFF00000) + 0x80000;

  process_table[pid].user_entry = entry_point;
  process_table[pid].user_stack = stack_addr;

  return pid;
}

static int sys_fs_read(const char *filename, unsigned char *buffer) {
  // Try RAM disk first
  int idx = fs_find(filename, current_dir_idx);
  if (idx != -1 && !files[idx].is_directory) {
    memcpy(buffer, files[idx].content, files[idx].size);
    buffer[files[idx].size] = '\0';
    return files[idx].size;
  }
  // Try FAT12 disk second
  return fat_read_file(filename, buffer);
}

void syscall_handler(registers_t *regs) {
  unsigned int sys_num = regs->eax;

  switch (sys_num) {
    case 1: // SYS_PRINT
      kprint((const char *)regs->ebx);
      break;

    case 2: // SYS_YIELD
      sys_yield();
      break;

    case 3: // SYS_EXIT
      process_table[current_process_id].state = 3; // TERMINATED
      sys_yield();
      break;

    case 4: // SYS_GFX_CLEAR
      wm_clear((unsigned char)regs->ebx);
      break;

    case 5: // SYS_GFX_DRAW_PIXEL
      wm_draw_pixel((int)regs->ebx, (int)regs->ecx, (unsigned char)regs->edx);
      break;

    case 6: // SYS_GFX_SWAP
      wm_swap();
      break;

    case 7: // SYS_GFX_DRAW_CHAR
      wm_draw_char((int)regs->ebx, (int)regs->ecx, (char)regs->edx, (unsigned char)regs->esi);
      break;

    case 8: // SYS_GFX_DRAW_STRING
      wm_draw_string((int)regs->ebx, (int)regs->ecx, (const char *)regs->edx, (unsigned char)regs->esi);
      break;

    case 9: // SYS_KEY_HAS
      regs->eax = ds_key_has();
      break;

    case 10: // SYS_KEY_READ
      regs->eax = ds_key_read();
      break;

    case 11: // SYS_MOUSE_GET
      {
        int clicks, mx, my;
        ds_mouse_get(&clicks, &mx, &my);
        regs->eax = clicks;
        regs->ebx = mx;
        regs->ecx = my;
      }
      break;

    case 12: // SYS_GET_TICKS
      regs->eax = ds_get_ticks();
      break;

    case 13: // SYS_FS_FIND
      regs->eax = fs_find((const char *)regs->ebx, current_dir_idx);
      break;

    case 14: // SYS_FS_READ
      regs->eax = sys_fs_read((const char *)regs->ebx, (unsigned char *)regs->ecx);
      break;

    case 15: // SYS_FS_WRITE
      regs->eax = fs_write_file((const char *)regs->ebx, (const char *)regs->ecx);
      break;

    case 16: // SYS_FS_DELETE
      regs->eax = fs_delete_file((const char *)regs->ebx);
      break;

    case 17: // SYS_FS_CREATE
      regs->eax = fs_create((const char *)regs->ebx, (int)regs->ecx);
      break;

    case 18: // SYS_SPAWN
      regs->eax = sys_spawn((const char *)regs->ebx);
      break;

    case 19: // SYS_GET_PROCESS_STATE
      {
        int pid = (int)regs->ebx;
        if (pid >= 0 && pid < MAX_PROCESSES) {
          regs->eax = process_table[pid].state;
        } else {
          regs->eax = 0; // UNUSED
        }
      }
      break;

    default:
      kprint("[SYSCALL] Unknown system call number: ");
      kprint_int(sys_num);
      kprint("\n");
      break;
  }
}
