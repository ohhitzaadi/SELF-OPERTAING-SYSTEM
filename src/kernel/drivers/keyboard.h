#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_handler();
int keyboard_has_key();
unsigned char keyboard_read_key();
char scancode_to_ascii(unsigned char scancode);

#endif
