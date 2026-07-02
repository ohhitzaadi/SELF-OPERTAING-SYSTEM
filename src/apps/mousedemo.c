#include "libuser.h"

void _start() {
  u_gfx_clear(0);

  u_gfx_draw_string(10, 10, "Mouse Demo (Draw with Left Click)", 14);
  u_gfx_draw_string(10, 22, "Right Click or press any key to exit", 10);

  // Draw canvas box (green outline)
  u_draw_rect(10, 35, 300, 145, 10);

  u_gfx_swap();

  while (1) {
    int clicks, mx, my;
    u_mouse_get(&clicks, &mx, &my);

    int click_l = clicks & 1;
    int click_r = clicks & 2;

    if (click_l) {
      if (mx > 10 && mx < 309 && my > 35 && my < 179) {
        u_gfx_draw_pixel(mx, my, 10); // Green dot
        u_gfx_draw_pixel(mx + 1, my, 10);
        u_gfx_draw_pixel(mx, my + 1, 10);
        u_gfx_draw_pixel(mx + 1, my + 1, 10);
      }
    }

    // Clear coordinates status bar area at the bottom
    u_draw_rect_filled(10, 185, 300, 10, 0);

    // Render coordinates and click status
    char status_str[80];
    u_strcpy(status_str, "X: ");
    char num[10];
    u_itoa(mx, num);
    u_strcpy(status_str + u_strlen(status_str), num);
    u_strcpy(status_str + u_strlen(status_str), " Y: ");
    u_itoa(my, num);
    u_strcpy(status_str + u_strlen(status_str), num);
    u_strcpy(status_str + u_strlen(status_str), " L: ");
    u_strcpy(status_str + u_strlen(status_str), click_l ? "Down" : "Up");
    u_strcpy(status_str + u_strlen(status_str), " R: ");
    u_strcpy(status_str + u_strlen(status_str), click_r ? "Down" : "Up");

    u_gfx_draw_string(12, 187, status_str, 15);

    u_gfx_swap();

    if (click_r) {
      break;
    }

    if (u_key_has()) {
      unsigned char scancode = u_key_read();
      if (scancode < 0x80) {
        break;
      }
    }

    // Delay to avoid spinning too fast
    for (volatile int d = 0; d < 30000; d++);
  }

  // Clear keyboard buffer before exiting
  while (u_key_has()) {
    u_key_read();
  }

  u_exit();
}
