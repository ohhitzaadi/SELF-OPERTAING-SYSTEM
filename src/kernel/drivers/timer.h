#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned int timer_ticks;

void timer_init(int hz);
void timer_handler();

#endif
