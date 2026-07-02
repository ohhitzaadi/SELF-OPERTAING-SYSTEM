#include "mouse.h"
#include "io.h"

volatile int mouse_x = 160;
volatile int mouse_y = 100;
volatile int mouse_click_l = 0;
volatile int mouse_click_r = 0;

static int mouse_cycle = 0;
static unsigned char mouse_bytes[3];

static void ps2_wait_write() {
  int timeout = 100000;
  while ((inb(0x64) & 2) != 0 && timeout > 0) {
    timeout--;
  }
}

static void ps2_wait_read() {
  int timeout = 100000;
  while ((inb(0x64) & 1) == 0 && timeout > 0) {
    timeout--;
  }
}

static void mouse_write(unsigned char data) {
  ps2_wait_write();
  outb(0x64, 0xD4); // Tell controller to address the mouse
  ps2_wait_write();
  outb(0x60, data);
}

static unsigned char mouse_read() {
  ps2_wait_read();
  return inb(0x60);
}

void mouse_init() {
  unsigned char status;

  // Enable auxiliary device
  ps2_wait_write();
  outb(0x64, 0xA8);

  // Read current controller configuration byte
  ps2_wait_write();
  outb(0x64, 0x20);
  ps2_wait_read();
  status = inb(0x60);

  // Enable mouse interrupts and clock line
  status |= 0x02;    // Enable mouse IRQ12 interrupt
  status &= ~0x20;   // Clear bit 5 to enable mouse clock

  // Write new configuration byte
  ps2_wait_write();
  outb(0x64, 0x60);
  ps2_wait_write();
  outb(0x60, status);

  // Set defaults
  mouse_write(0xF6);
  mouse_read(); // Read ACK (0xFA)

  // Enable packet data reporting
  mouse_write(0xF4);
  mouse_read(); // Read ACK (0xFA)
}

void mouse_handler() {
  unsigned char status = inb(0x64);
  // Check if buffer contains auxiliary (mouse) data
  if (status & 0x01) {
    unsigned char val = inb(0x60);
    if (mouse_cycle == 0) {
      if (val & 0x08) {
        mouse_bytes[0] = val;
        mouse_cycle = 1;
      }
    } else if (mouse_cycle == 1) {
      mouse_bytes[1] = val;
      mouse_cycle = 2;
    } else if (mouse_cycle == 2) {
      mouse_bytes[2] = val;
      mouse_cycle = 0;

      int x_sign = (mouse_bytes[0] & 0x10) ? 1 : 0;
      int y_sign = (mouse_bytes[0] & 0x20) ? 1 : 0;

      int dx = mouse_bytes[1];
      int dy = mouse_bytes[2];

      if (x_sign) {
        dx -= 256;
      }
      if (y_sign) {
        dy -= 256;
      }

      // Delta Y is inverted on screen vs mouse
      dy = -dy;

      mouse_x += dx;
      mouse_y += dy;

      if (mouse_x < 0) mouse_x = 0;
      if (mouse_x >= 320) mouse_x = 319;
      if (mouse_y < 0) mouse_y = 0;
      if (mouse_y >= 200) mouse_y = 199;

      mouse_click_l = (mouse_bytes[0] & 0x01) ? 1 : 0;
      mouse_click_r = (mouse_bytes[0] & 0x02) ? 1 : 0;
    }
  }
  // Send EOI to both Slave and Master PIC
  outb(0xA0, 0x20);
  outb(0x20, 0x20);
}
