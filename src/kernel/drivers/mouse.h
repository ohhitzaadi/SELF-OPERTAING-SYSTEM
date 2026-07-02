#ifndef MOUSE_H
#define MOUSE_H

extern volatile int mouse_x;
extern volatile int mouse_y;
extern volatile int mouse_click_l;
extern volatile int mouse_click_r;

void mouse_init();
void mouse_handler();

#endif
