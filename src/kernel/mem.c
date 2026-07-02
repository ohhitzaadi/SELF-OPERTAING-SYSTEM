#include "mem.h"

// For diagnostic logging
extern void kprint(const char *str);
extern void kprint_int(int n);

// String utilities implementation
size_t strlen(const char *s) {
  size_t len = 0;
  while (s[len])
    len++;
  return len;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

void *memset(void *dest, int val, size_t count) {
  unsigned char *temp = (unsigned char *)dest;
  for (size_t i = 0; i < count; i++) {
    temp[i] = (unsigned char)val;
  }
  return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  if (d < s) {
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = n; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }
  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

void itoa(int n, char str[]) {
  int i, sign;
  if ((sign = n) < 0)
    n = -n;
  i = 0;
  do {
    str[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);
  if (sign < 0)
    str[i++] = '-';
  str[i] = '\0';

  // Reverse the string
  int j;
  char temp;
  for (j = 0; j < i / 2; j++) {
    temp = str[j];
    str[j] = str[i - j - 1];
    str[i - j - 1] = temp;
  }
}

// Dynamic Memory Heap Allocator implementation
unsigned char heap_memory[HEAP_SIZE];

typedef struct heap_block {
  size_t size;
  int is_free;
  struct heap_block *next;
} heap_block_t;

heap_block_t *heap_start = NULL;

void heap_init() {
  heap_start = (heap_block_t *)heap_memory;
  heap_start->size = HEAP_SIZE - sizeof(heap_block_t);
  heap_start->is_free = 1;
  heap_start->next = NULL;
}

void *malloc(size_t size) {
  if (heap_start == NULL) {
    heap_init();
  }

  // Align to 4-byte boundaries
  size = (size + 3) & ~3;

  heap_block_t *curr = heap_start;
  while (curr != NULL) {
    if (curr->is_free && curr->size >= size) {
      // Split block if there's enough space left
      if (curr->size >= size + sizeof(heap_block_t) + 4) {
        heap_block_t *next_block =
            (heap_block_t *)((unsigned char *)curr + sizeof(heap_block_t) +
                             size);
        next_block->size = curr->size - size - sizeof(heap_block_t);
        next_block->is_free = 1;
        next_block->next = curr->next;

        curr->size = size;
        curr->next = next_block;
      }
      curr->is_free = 0;
      return (void *)((unsigned char *)curr + sizeof(heap_block_t));
    }
    curr = curr->next;
  }
  return NULL; // Out of memory
}

void free(void *ptr) {
  if (ptr == NULL)
    return;

  heap_block_t *block =
      (heap_block_t *)((unsigned char *)ptr - sizeof(heap_block_t));
  block->is_free = 1;

  // Coalesce adjacent free blocks
  heap_block_t *curr = heap_start;
  while (curr != NULL) {
    while (curr->is_free && curr->next != NULL && curr->next->is_free) {
      curr->size += sizeof(heap_block_t) + curr->next->size;
      curr->next = curr->next->next;
    }
    curr = curr->next;
  }
}

// Physical Memory Manager
unsigned int pmm_bitmap[NUM_FRAMES / 32];

void pmm_init() {
  memset(pmm_bitmap, 0, sizeof(pmm_bitmap));
  // Mark the first 1MB of memory (256 frames) as allocated (kernel, GDT, stacks)
  for (int i = 0; i < 256; i++) {
    pmm_bitmap[i / 32] |= (1 << (i % 32));
  }
}

unsigned int pmm_alloc_frame() {
  for (int i = 0; i < NUM_FRAMES / 32; i++) {
    if (pmm_bitmap[i] != 0xFFFFFFFF) {
      for (int j = 0; j < 32; j++) {
        if (!(pmm_bitmap[i] & (1 << j))) {
          int frame = i * 32 + j;
          pmm_bitmap[i] |= (1 << j);
          return frame * PAGE_SIZE; // Returns physical address
        }
      }
    }
  }
  return 0; // OOM
}

void pmm_free_frame(unsigned int phys_addr) {
  int frame = phys_addr / PAGE_SIZE;
  pmm_bitmap[frame / 32] &= ~(1 << (frame % 32));
}

// Virtual Memory Manager
unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int kernel_page_table_1[1024] __attribute__((aligned(4096)));
unsigned int kernel_page_table_2[1024] __attribute__((aligned(4096)));
unsigned int kernel_page_table_3[1024] __attribute__((aligned(4096)));

void vmm_init() {
  // Identity map the first 12MB of physical RAM
  // PDE/PTE Flags: Present (0x01) | Read-Write (0x02) | User (0x04) = 0x07
  for (int i = 0; i < 1024; i++) {
    kernel_page_table_1[i] = (i * 4096) | 0x07;
    kernel_page_table_2[i] = (0x400000 + i * 4096) | 0x07;
    kernel_page_table_3[i] = (0x800000 + i * 4096) | 0x07;
  }

  // Link Page Tables to Page Directory
  page_directory[0] = ((uintptr_t)kernel_page_table_1) | 0x07;
  page_directory[1] = ((uintptr_t)kernel_page_table_2) | 0x07;
  page_directory[2] = ((uintptr_t)kernel_page_table_3) | 0x07;

  for (int i = 3; i < 1024; i++) {
    page_directory[i] = 0x00;
  }

  // Load PD Address in CR3
  uintptr_t pd_addr = (uintptr_t)page_directory;
  __asm__ volatile("mov %0, %%cr3" : : "r"(pd_addr));

  // Enable x86 Paging
  unsigned int cr0;
  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000;
  __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

  kprint("[MEM] Paging enabled. Initial 12MB identity mapped with user access.\n");
}

void vmm_map_page(unsigned int virtual_addr, unsigned int physical_addr) {
  unsigned int pd_index = virtual_addr >> 22;
  unsigned int pt_index = (virtual_addr >> 12) & 0x3FF;

  if (!(page_directory[pd_index] & 0x01)) {
    unsigned int new_pt_phys = pmm_alloc_frame();
    if (!new_pt_phys) {
      kprint("[ERROR] Out of physical RAM for new page table!\n");
      return;
    }
    unsigned int *new_pt = (unsigned int *)(uintptr_t)new_pt_phys;
    for (int i = 0; i < 1024; i++) {
      new_pt[i] = 0x00;
    }
    page_directory[pd_index] = new_pt_phys | 0x07;
  }

  unsigned int *pt =
      (unsigned int *)(uintptr_t)(page_directory[pd_index] & 0xFFFFF000);
  pt[pt_index] = (physical_addr & 0xFFFFF000) | 0x07;

  // Invalidate TLB for mapped address
  __asm__ volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}
