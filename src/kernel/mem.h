#ifndef MEM_H
#define MEM_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096
#define PHYS_MEM_SIZE (32 * 1024 * 1024)       // 32 MB
#define NUM_FRAMES (PHYS_MEM_SIZE / PAGE_SIZE) // 8192 frames

// Heap size (2MB heap)
#define HEAP_SIZE (1024 * 1024 * 2)

// String and helper functions
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
void *memset(void *dest, int val, size_t count);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void itoa(int n, char str[]);

// PMM
void pmm_init();
unsigned int pmm_alloc_frame();
void pmm_free_frame(unsigned int phys_addr);

// VMM
extern unsigned int page_directory[1024];
void vmm_init();
void vmm_map_page(unsigned int virtual_addr, unsigned int physical_addr);

// Heap
void heap_init();
void *malloc(size_t size);
void free(void *ptr);

#endif
