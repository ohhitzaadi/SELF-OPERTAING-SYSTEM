#include "libuser.h"

// System Call Helper Macros
static int syscall0(int num) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num));
  return ret;
}

static int syscall1(int num, int arg1) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
  return ret;
}

static int syscall2(int num, int arg1, int arg2) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
  return ret;
}

static int __attribute__((unused)) syscall3(int num, int arg1, int arg2, int arg3) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
  return ret;
}

static int syscall4(int num, int arg1, int arg2, int arg3, int arg4) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
  return ret;
}

// String functions
size_t u_strlen(const char *s) {
  size_t len = 0;
  while (s[len]) len++;
  return len;
}

int u_strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++; s2++;
  }
  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *u_strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++));
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

void u_itoa(int n, char str[]) {
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

  int j;
  char temp;
  for (j = 0; j < i / 2; j++) {
    temp = str[j];
    str[j] = str[i - j - 1];
    str[i - j - 1] = temp;
  }
}

// API wrappers
void u_print(const char *str) {
  syscall1(1, (int)str);
}

void u_yield() {
  syscall0(2);
}

void u_exit() {
  syscall0(3);
}

void u_gfx_clear(unsigned char color) {
  for (int i = 0; i < 320 * 200; i++) {
    backbuffer[i] = color;
  }
}

void u_gfx_draw_pixel(int x, int y, unsigned char color) {
  if (x >= 0 && x < 320 && y >= 0 && y < 200) {
    backbuffer[y * 320 + x] = color;
  }
}

void u_gfx_swap() {
  syscall0(6);
}

void u_gfx_draw_char(int x, int y, char c, unsigned char color) {
  syscall4(7, x, y, (int)c, (int)color);
}

void u_gfx_draw_string(int x, int y, const char *str, unsigned char color) {
  syscall4(8, x, y, (int)str, (int)color);
}

int u_key_has() {
  return syscall0(9);
}

unsigned char u_key_read() {
  return (unsigned char)syscall0(10);
}

void u_mouse_get(int *clicks, int *mx, int *my) {
  int cl, x, y;
  __asm__ volatile("int $0x80"
                   : "=a"(cl), "=b"(x), "=c"(y)
                   : "a"(11));
  if (clicks) *clicks = cl;
  if (mx) *mx = x;
  if (my) *my = y;
}

unsigned int u_get_ticks() {
  return (unsigned int)syscall0(12);
}

int u_fs_find(const char *name) {
  return syscall1(13, (int)name);
}

int u_fs_read(const char *name, unsigned char *buffer) {
  return syscall2(14, (int)name, (int)buffer);
}

int u_fs_write(const char *name, const char *content) {
  return syscall2(15, (int)name, (int)content);
}

int u_fs_delete(const char *name) {
  return syscall1(16, (int)name);
}

int u_fs_create(const char *name, int is_dir) {
  return syscall2(17, (int)name, is_dir);
}

int u_spawn(const char *filename) {
  return syscall1(18, (int)filename);
}

int u_get_process_state(int pid) {
  return syscall1(19, pid);
}

// User-space drawing logic
void u_draw_rect(int x, int y, int w, int h, unsigned char color) {
  for (int i = 0; i < w; i++) {
    u_gfx_draw_pixel(x + i, y, color);
    u_gfx_draw_pixel(x + i, y + h - 1, color);
  }
  for (int i = 0; i < h; i++) {
    u_gfx_draw_pixel(x, y + i, color);
    u_gfx_draw_pixel(x + w - 1, y + i, color);
  }
}

void u_draw_rect_filled(int x, int y, int w, int h, unsigned char color) {
  for (int r = 0; r < h; r++) {
    for (int c = 0; c < w; c++) {
      u_gfx_draw_pixel(x + c, y + r, color);
    }
  }
}

void u_draw_circle(int xc, int yc, int r, unsigned char color) {
  int x = 0;
  int y = r;
  int d = 3 - 2 * r;

  while (y >= x) {
    u_gfx_draw_pixel(xc + x, yc + y, color);
    u_gfx_draw_pixel(xc - x, yc + y, color);
    u_gfx_draw_pixel(xc + x, yc - y, color);
    u_gfx_draw_pixel(xc - x, yc - y, color);
    u_gfx_draw_pixel(xc + y, yc + x, color);
    u_gfx_draw_pixel(xc - y, yc + x, color);
    u_gfx_draw_pixel(xc + y, yc - x, color);
    u_gfx_draw_pixel(xc - y, yc - x, color);
    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    } else {
      d = d + 4 * x + 6;
    }
  }
}

void u_draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
  int dx = x1 < x2 ? x2 - x1 : x1 - x2;
  int dy = y1 < y2 ? y2 - y1 : y1 - y2;
  int sx = x1 < x2 ? 1 : -1;
  int sy = y1 < y2 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2;
  int e2;

  while (1) {
    u_gfx_draw_pixel(x1, y1, color);
    if (x1 == x2 && y1 == y2)
      break;
    e2 = err;
    if (e2 > -dx) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dy) {
      err += dx;
      y1 += sy;
    }
  }
}
