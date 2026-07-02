#ifndef LIBUSER_H
#define LIBUSER_H

#include <stddef.h>

#define BACKBUFFER_ADDRESS 0x300000
#define backbuffer ((unsigned char *)BACKBUFFER_ADDRESS)

// String Helpers
size_t u_strlen(const char *s);
int u_strcmp(const char *s1, const char *s2);
char *u_strcpy(char *dest, const char *src);
void *memset(void *dest, int val, size_t count);
void *memcpy(void *dest, const void *src, size_t n);
void u_itoa(int n, char str[]);

// Syscall API Wrappers (via int 0x80)
void u_print(const char *str);
void u_yield();
void u_exit();
void u_gfx_clear(unsigned char color);
void u_gfx_draw_pixel(int x, int y, unsigned char color);
void u_gfx_swap();
void u_gfx_draw_char(int x, int y, char c, unsigned char color);
void u_gfx_draw_string(int x, int y, const char *str, unsigned char color);
int u_key_has();
unsigned char u_key_read();
void u_mouse_get(int *clicks, int *mx, int *my);
unsigned int u_get_ticks();
int u_fs_find(const char *name);
int u_fs_read(const char *name, unsigned char *buffer);
int u_fs_write(const char *name, const char *content);
int u_fs_delete(const char *name);
int u_fs_create(const char *name, int is_dir);
int u_spawn(const char *filename);
int u_get_process_state(int pid);

// Library drawing utilities (implemented in user space)
void u_draw_rect(int x, int y, int w, int h, unsigned char color);
void u_draw_rect_filled(int x, int y, int w, int h, unsigned char color);
void u_draw_circle(int xc, int yc, int r, unsigned char color);
void u_draw_line(int x1, int y1, int x2, int y2, unsigned char color);

#endif
