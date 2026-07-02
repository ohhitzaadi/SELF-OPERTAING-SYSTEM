#include "sched.h"
#include "mem.h" // IWYU pragma: keep

pcb_t process_table[MAX_PROCESSES];
int current_process_id = 0;

extern void switch_to_task(unsigned int *old_esp, unsigned int new_esp);

void sched_init() {
  memset(process_table, 0, sizeof(process_table));
}

int create_process(void (*entry_point)(), const char *name) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (process_table[i].state == 0) { // UNUSED
      pcb_t *p = &process_table[i];
      p->pid = i;
      p->state = 1; // READY

      int name_len = strlen(name);
      int k;
      for (k = 0; k < name_len && k < 31; k++) {
        p->name[k] = name[k];
      }
      p->name[k] = '\0';

      // Initialize stack.
      // switch_to_task pops: edi, esi, ebx, ebp, then ret.
      int top = STACK_SIZE - 1;
      p->stack[top] = (uintptr_t)entry_point; // Return address (EIP)
      p->stack[top - 1] = 0;                  // EBP
      p->stack[top - 2] = 0;                  // EBX
      p->stack[top - 3] = 0;                  // ESI
      p->stack[top - 4] = 0;                  // EDI

      p->esp = (uintptr_t)&p->stack[top - 4];
      return i;
    }
  }
  return -1; // Out of process slots
}

void sys_yield() {
  int next_pid = -1;
  int curr_pid = current_process_id;

  // Find the next READY process (Round-Robin)
  for (int i = 1; i <= MAX_PROCESSES; i++) {
    int idx = (curr_pid + i) % MAX_PROCESSES;
    if (process_table[idx].state == 1) { // READY
      next_pid = idx;
      break;
    }
  }

  if (next_pid != -1 && next_pid != curr_pid) {
    // Transition current RUNNING process back to READY (if not exited)
    if (process_table[curr_pid].state == 2) {
      if (curr_pid == 1) {
        process_table[curr_pid].state = 0; // UNUSED (dummy boot process)
      } else {
        process_table[curr_pid].state = 1; // READY
      }
    }

    process_table[next_pid].state = 2; // Set next to RUNNING
    current_process_id = next_pid;

    // Call the assembly switcher
    switch_to_task(&(process_table[curr_pid].esp), process_table[next_pid].esp);
  }
}

extern void enter_user_mode(unsigned int entry, unsigned int stack);

void user_mode_trampoline() {
  pcb_t *p = &process_table[current_process_id];
  enter_user_mode(p->user_entry, p->user_stack);
}

