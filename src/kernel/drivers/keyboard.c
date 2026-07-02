#include "keyboard.h"
#include "io.h"

#define KEY_BUFFER_SIZE 64
volatile unsigned char key_buffer[KEY_BUFFER_SIZE];
volatile int key_head = 0;
volatile int key_tail = 0;

void keyboard_handler() {
  unsigned char scancode = inb(0x60);
  int next = (key_head + 1) % KEY_BUFFER_SIZE;
  if (next != key_tail) {
    key_buffer[key_head] = scancode;
    key_head = next;
  }
  outb(0x20, 0x20); // Send EOI to PIC
}

int keyboard_has_key() { return key_head != key_tail; }

unsigned char keyboard_read_key() {
  if (key_head == key_tail)
    return 0;
  unsigned char code = key_buffer[key_tail];
  key_tail = (key_tail + 1) % KEY_BUFFER_SIZE;
  return code;
}

char scancode_to_ascii(unsigned char scancode) {
  switch (scancode) {
  case 0x02: return '1';
  case 0x03: return '2';
  case 0x04: return '3';
  case 0x05: return '4';
  case 0x06: return '5';
  case 0x07: return '6';
  case 0x08: return '7';
  case 0x09: return '8';
  case 0x0A: return '9';
  case 0x0B: return '0';
  case 0x0C: return '-';
  case 0x0D: return '=';
  case 0x0E: return '\b'; // Backspace
  case 0x0F: return '\t'; // Tab
  case 0x10: return 'q';
  case 0x11: return 'w';
  case 0x12: return 'e';
  case 0x13: return 'r';
  case 0x14: return 't';
  case 0x15: return 'y';
  case 0x16: return 'u';
  case 0x17: return 'i';
  case 0x18: return 'o';
  case 0x19: return 'p';
  case 0x1A: return '[';
  case 0x1B: return ']';
  case 0x1C: return '\n'; // Enter
  case 0x1E: return 'a';
  case 0x1F: return 's';
  case 0x20: return 'd';
  case 0x21: return 'f';
  case 0x22: return 'g';
  case 0x23: return 'h';
  case 0x24: return 'j';
  case 0x25: return 'k';
  case 0x26: return 'l';
  case 0x27: return ';';
  case 0x28: return '\'';
  case 0x29: return '`';
  case 0x2C: return 'z';
  case 0x2D: return 'x';
  case 0x2E: return 'c';
  case 0x2F: return 'v';
  case 0x30: return 'b';
  case 0x31: return 'n';
  case 0x32: return 'm';
  case 0x33: return ',';
  case 0x34: return '.';
  case 0x35: return '/';
  case 0x39: return ' '; // Space
  default: return 0;
  }
}
