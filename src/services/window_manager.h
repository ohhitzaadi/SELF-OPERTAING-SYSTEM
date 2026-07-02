#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

#define BACKBUFFER_ADDRESS 0x300000
#define backbuffer ((unsigned char *)BACKBUFFER_ADDRESS)

void wm_clear(unsigned char color);
void wm_draw_pixel(int x, int y, unsigned char color);
void wm_swap();
void wm_draw_char(int x, int y, char c, unsigned char color);
void wm_draw_string(int x, int y, const char *str, unsigned char color);
void wm_draw_line(int x1, int y1, int x2, int y2, unsigned char color);
void wm_draw_circle(int xc, int yc, int r, unsigned char color);
void wm_draw_rect(int x, int y, int w, int h, unsigned char color);
void wm_draw_rect_filled(int x, int y, int w, int h, unsigned char color);

#endif
