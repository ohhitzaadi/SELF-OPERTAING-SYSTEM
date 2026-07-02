#ifndef SCHED_H
#define SCHED_H

#define MAX_PROCESSES 8
#define STACK_SIZE 2048

typedef struct {
  int pid;
  unsigned int esp;
  int state; // 0=UNUSED, 1=READY, 2=RUNNING, 3=TERMINATED
  char name[32];
  unsigned int stack[STACK_SIZE];
  unsigned int user_entry;
  unsigned int user_stack;
} pcb_t;

extern pcb_t process_table[MAX_PROCESSES];
extern int current_process_id;

void sched_init();
int create_process(void (*entry_point)(), const char *name);
void sys_yield();

#endif
