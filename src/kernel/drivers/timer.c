#include "timer.h"
#include "io.h"

volatile unsigned int timer_ticks = 0;

void timer_handler() {
  timer_ticks++;
  outb(0x20, 0x20); // Send EOI to PIC
}

void timer_init(int hz) {
  int divisor = 1193180 / hz;
  outb(0x43, 0x36);
  outb(0x40, divisor & 0xFF);
  outb(0x40, (divisor >> 8) & 0xFF);
}
