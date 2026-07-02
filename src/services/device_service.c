#include "device_service.h"
#include "kernel/drivers/keyboard.h"
#include "kernel/drivers/mouse.h"
#include "kernel/drivers/timer.h"

int ds_key_has() {
  return keyboard_has_key();
}

unsigned char ds_key_read() {
  return keyboard_read_key();
}

char ds_scancode_to_ascii(unsigned char scancode) {
  return scancode_to_ascii(scancode);
}

void ds_mouse_get(int *clicks, int *mx, int *my) {
  if (clicks) {
    *clicks = mouse_click_l | (mouse_click_r << 1);
  }
  if (mx) {
    *mx = mouse_x;
  }
  if (my) {
    *my = mouse_y;
  }
}

unsigned int ds_get_ticks() {
  return timer_ticks;
}
