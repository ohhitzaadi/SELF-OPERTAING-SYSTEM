#ifndef DEVICE_SERVICE_H
#define DEVICE_SERVICE_H

int ds_key_has();
unsigned char ds_key_read();
char ds_scancode_to_ascii(unsigned char scancode);

void ds_mouse_get(int *clicks, int *mx, int *my);

unsigned int ds_get_ticks();

#endif
