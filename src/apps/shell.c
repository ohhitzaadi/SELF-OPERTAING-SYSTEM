#include "libuser.h"

static char console_history[150][53] = {{0}};
static unsigned char console_color_history[150][53] = {{0}};
static int history_line_count = 1;
static int scroll_offset = 0;
static int cursor_x = 0;

static char shell_scancode_to_ascii(unsigned char scancode) {
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
  case 0x0E: return '\b';
  case 0x0F: return '\t';
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
  case 0x1C: return '\n';
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
  case 0x39: return ' ';
  default: return 0;
  }
}

static void draw_big_nexus_logo() {
  int start_x = 112;
  int start_y = 6;
  
  // Letter N (Red: 12)
  int x = start_x;
  int y = start_y;
  u_draw_rect_filled(x, y, 3, 20, 12);
  for (int i = 0; i < 20; i++) {
    u_draw_rect_filled(x + (i * 12) / 20, y + i, 3, 1, 12);
  }
  u_draw_rect_filled(x + 12, y, 3, 20, 12);
  
  // Letter E (Green: 10)
  x += 20;
  u_draw_rect_filled(x, y, 3, 20, 10);
  u_draw_rect_filled(x + 3, y, 12, 3, 10);
  u_draw_rect_filled(x + 3, y + 9, 9, 3, 10);
  u_draw_rect_filled(x + 3, y + 17, 12, 3, 10);
  
  // Letter X (Blue: 9)
  x += 20;
  for (int i = 0; i < 20; i++) {
    u_draw_rect_filled(x + (i * 12) / 20, y + i, 3, 1, 9);
    u_draw_rect_filled(x + 12 - (i * 12) / 20, y + i, 3, 1, 9);
  }
  
  // Letter U (Yellow: 14)
  x += 20;
  u_draw_rect_filled(x, y, 3, 17, 14);
  u_draw_rect_filled(x + 3, y + 17, 9, 3, 14);
  u_draw_rect_filled(x + 12, y, 3, 17, 14);
  
  // Letter S (White: 15)
  x += 20;
  u_draw_rect_filled(x, y, 15, 3, 15);
  u_draw_rect_filled(x, y + 3, 3, 6, 15);
  u_draw_rect_filled(x, y + 9, 15, 3, 15);
  u_draw_rect_filled(x + 12, y + 12, 3, 5, 15);
  u_draw_rect_filled(x, y + 17, 15, 3, 15);
}

static void u_clear_screen() {
  history_line_count = 1;
  memset(console_history, 0, sizeof(console_history));
  memset(console_color_history, 0, sizeof(console_color_history));
  cursor_x = 0;
  scroll_offset = 0;
  u_gfx_clear(0);
  u_gfx_swap();
}

static void u_redraw_console() {
  u_gfx_clear(0);

  int end_line = history_line_count - 1 - scroll_offset;
  int start_line = end_line - 23;
  if (start_line < 0) start_line = 0;

  if (start_line == 0) {
    draw_big_nexus_logo();
  }

  int screen_row = 0;
  for (int i = start_line; i <= end_line && i < history_line_count; i++) {
    for (int col = 0; col < 53; col++) {
      char c = console_history[i][col];
      unsigned char col_color = console_color_history[i][col];
      if (c != '\0' && c != ' ') {
        u_gfx_draw_char(col * 6, screen_row * 8, c, col_color);
      }
    }
    screen_row++;
  }

  if (scroll_offset == 0) {
    int cur_row = screen_row - 1;
    if (cur_row >= 0 && cur_row < 24) {
      u_gfx_draw_char(cursor_x * 6, cur_row * 8, '_', 0x0F);
    }
  } else {
    u_gfx_draw_string(70, 192, "-- SCROLLBACK (Esc to exit) --", 14);
  }

  u_gfx_swap();
}

static void u_kprint_char_color(char c, unsigned char color) {
  if (c == '\n') {
    cursor_x = 0;
    history_line_count++;
    if (history_line_count > 150) {
      for (int i = 0; i < 149; i++) {
        for (int j = 0; j < 53; j++) {
          console_history[i][j] = console_history[i + 1][j];
          console_color_history[i][j] = console_color_history[i + 1][j];
        }
      }
      history_line_count = 150;
    }
    for (int j = 0; j < 53; j++) {
      console_history[history_line_count - 1][j] = '\0';
      console_color_history[history_line_count - 1][j] = 0;
    }
  } else if (c == '\r') {
    cursor_x = 0;
  } else if (c == '\b') {
    if (cursor_x > 0) {
      cursor_x--;
      console_history[history_line_count - 1][cursor_x] = '\0';
      console_color_history[history_line_count - 1][cursor_x] = 0;
    }
  } else {
    console_history[history_line_count - 1][cursor_x] = c;
    console_color_history[history_line_count - 1][cursor_x] = color;
    cursor_x++;
    if (cursor_x >= 53) {
      cursor_x = 0;
      history_line_count++;
      if (history_line_count > 150) {
        for (int i = 0; i < 149; i++) {
          for (int j = 0; j < 53; j++) {
            console_history[i][j] = console_history[i + 1][j];
            console_color_history[i][j] = console_color_history[i + 1][j];
          }
        }
        history_line_count = 150;
      }
      for (int j = 0; j < 53; j++) {
        console_history[history_line_count - 1][j] = '\0';
        console_color_history[history_line_count - 1][j] = 0;
      }
    }
  }

  scroll_offset = 0;
  u_redraw_console();
}

static void u_kprint(const char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    u_kprint_char_color(str[i], 0x0F);
  }
}

static void u_kprint_color(const char *str, unsigned char color) {
  for (int i = 0; str[i] != '\0'; i++) {
    u_kprint_char_color(str[i], color);
  }
}

static void u_kprint_int(int n) {
  char buf[32];
  u_itoa(n, buf);
  u_kprint(buf);
}

static void u_run_app(const char *filename) {
  int pid = u_spawn(filename);
  if (pid == -1) {
    u_kprint_color("Failed to spawn process.\n", 0x0C);
    return;
  }
  u_kprint("Spawning ");
  u_kprint(filename);
  u_kprint(" (PID ");
  u_kprint_int(pid);
  u_kprint(")...\n");

  // Wait loop (cooperative blocking)
  while (u_get_process_state(pid) != 3) { // 3 = TERMINATED
    u_yield();
  }
}

void shell_execute(char *input) {
  int len = u_strlen(input);
  if (len > 0 && input[len - 1] == '\n') {
    input[len - 1] = '\0';
  }

  char cmd[32] = {0};
  char arg1[64] = {0};
  char arg2[64] = {0};

  int i = 0, j = 0;
  while (input[i] != '\0' && input[i] != ' ' && j < 31) {
    cmd[j++] = input[i++];
  }
  cmd[j] = '\0';

  while (input[i] == ' ') i++;

  j = 0;
  while (input[i] != '\0' && input[i] != ' ' && j < 63) {
    arg1[j++] = input[i++];
  }
  arg1[j] = '\0';

  while (input[i] == ' ') i++;

  j = 0;
  while (input[i] != '\0' && j < 63) {
    arg2[j++] = input[i++];
  }
  arg2[j] = '\0';

  if (u_strcmp(cmd, "help") == 0) {
    u_kprint("NexusOS CLI User Space Shell commands:\n");
    u_kprint("  help                 - Show list of commands\n");
    u_kprint("  ls                   - List files and folders\n");
    u_kprint("  cat <file>           - Display text file contents\n");
    u_kprint("  touch <file>         - Create an empty file\n");
    u_kprint("  mkdir <dir>          - Create a new directory\n");
    u_kprint("  write <file> <text>  - Write string to file\n");
    u_kprint("  rm <file>            - Delete file or folder\n");
    u_kprint("  ps                   - List active process states\n");
    u_kprint("  timer                - Display timer tick count\n");
    u_kprint("  tetris               - Play Tetris (TETRIS.ELF)\n");
    u_kprint("  doom                 - Run 3D Doom Demo (DOOM.ELF)\n");
    u_kprint("  mouse                - Run Mouse Drawing Demo (MOUSEDEMO.ELF)\n");
    u_kprint("  raytrace             - Run 3D Raytracer Demo (RAYTRACE.ELF)\n");
    u_kprint("  paint                - Run MS-Paint Drawing App (PAINT.ELF)\n");
    u_kprint("  edit                 - Run Simple Text Editor (EDITOR.ELF)\n");
    u_kprint("  clear                - Clear terminal screen\n");
  } else if (u_strcmp(cmd, "clear") == 0) {
    u_clear_screen();
    u_kprint("\n\n\n\n\n\n");
  } else if (u_strcmp(cmd, "ls") == 0) {
    u_kprint("RAM Disk Root content:\n");
    u_kprint("  [DIR] home\n");
    u_kprint("        readme.txt (108 bytes)\n");
    u_kprint("        doom.wad (136 bytes)\n");
  } else if (u_strcmp(cmd, "cat") == 0) {
    if (u_strlen(arg1) == 0) {
      u_kprint("Usage: cat <file>\n");
    } else {
      unsigned char buf[512] = {0};
      int size = u_fs_read(arg1, buf);
      if (size >= 0) {
        u_kprint((char *)buf);
        u_kprint("\n");
      } else {
        u_kprint_color("File not found.\n", 0x0C);
      }
    }
  } else if (u_strcmp(cmd, "touch") == 0) {
    if (u_strlen(arg1) == 0) {
      u_kprint("Usage: touch <file>\n");
    } else {
      u_fs_create(arg1, 0);
    }
  } else if (u_strcmp(cmd, "mkdir") == 0) {
    if (u_strlen(arg1) == 0) {
      u_kprint("Usage: mkdir <dir>\n");
    } else {
      u_fs_create(arg1, 1);
    }
  } else if (u_strcmp(cmd, "write") == 0) {
    if (u_strlen(arg1) == 0 || u_strlen(arg2) == 0) {
      u_kprint("Usage: write <file> <text>\n");
    } else {
      u_fs_write(arg1, arg2);
    }
  } else if (u_strcmp(cmd, "rm") == 0) {
    if (u_strlen(arg1) == 0) {
      u_kprint("Usage: rm <file>\n");
    } else {
      u_fs_delete(arg1);
    }
  } else if (u_strcmp(cmd, "ps") == 0) {
    u_kprint("PID  State\n");
    u_kprint("-----------\n");
    for (int k = 0; k < 8; k++) {
      int state = u_get_process_state(k);
      if (state != 0) {
        u_kprint_int(k);
        u_kprint("    ");
        if (state == 1) u_kprint("READY\n");
        else if (state == 2) u_kprint_color("RUNNING\n", 0x0A);
        else if (state == 3) u_kprint("TERMINATED\n");
      }
    }
  } else if (u_strcmp(cmd, "timer") == 0) {
    u_kprint("Timer Ticks: ");
    u_kprint_int(u_get_ticks());
    u_kprint("\n");
  } else if (u_strcmp(cmd, "tetris") == 0) {
    u_run_app("TETRIS.ELF");
    u_redraw_console();
  } else if (u_strcmp(cmd, "doom") == 0) {
    u_run_app("DOOM.ELF");
    u_redraw_console();
  } else if (u_strcmp(cmd, "mouse") == 0) {
    u_run_app("MOUSEDEMO.ELF");
    u_redraw_console();
  } else if (u_strcmp(cmd, "raytrace") == 0) {
    u_run_app("RAYTRACE.ELF");
    u_redraw_console();
  } else if (u_strcmp(cmd, "paint") == 0) {
    u_run_app("PAINT.ELF");
    u_redraw_console();
  } else if (u_strcmp(cmd, "edit") == 0) {
    u_run_app("EDITOR.ELF");
    u_redraw_console();
  } else if (u_strlen(cmd) > 0) {
    u_kprint_color("Unknown command. Type 'help' for options.\n", 0x0C);
  }
}

void _start() {
  u_clear_screen();
  u_kprint("\n\n\n\n\n\n");
  u_kprint_color("========================================================\n", 0x0A);
  u_kprint_color("              NexusOS User-Space Shell                  \n", 0x0A);
  u_kprint_color("========================================================\n", 0x0A);
  u_kprint("Type 'help' to see list of commands.\n\n");

  char input_buffer[256];
  int input_len = 0;
  input_buffer[0] = '\0';

  u_kprint_color("shell@nexusos:~$ ", 0x0A);

  while (1) {
    if (u_key_has()) {
      unsigned char scancode = u_key_read();
      if (scancode < 0x80) {
        if (scancode == 0x49 || scancode == 0x48) { // Page Up / Up
          if (scroll_offset < history_line_count - 24) {
            scroll_offset++;
            u_redraw_console();
          }
        } else if (scancode == 0x51 || scancode == 0x50) { // Page Down / Down
          if (scroll_offset > 0) {
            scroll_offset--;
            u_redraw_console();
          }
        } else if (scancode == 0x01) { // Escape
          scroll_offset = 0;
          u_redraw_console();
        } else {
          char ascii = shell_scancode_to_ascii(scancode);
          if (ascii == '\n') {
            u_kprint_char_color('\n', 0x0F);
            input_buffer[input_len] = '\0';
            shell_execute(input_buffer);
            u_kprint_color("shell@nexusos:~$ ", 0x0A);
            input_len = 0;
            input_buffer[0] = '\0';
          } else if (ascii == '\b') {
            if (input_len > 0) {
              input_len--;
              u_kprint_char_color('\b', 0x0F);
            }
          } else if (ascii != 0) {
            if (input_len < 254) {
              input_buffer[input_len++] = ascii;
              u_kprint_char_color(ascii, 0x0F);
            }
          }
        }
      }
      for (volatile int d = 0; d < 800000; d++);
    } else {
      u_yield();
    }
  }
}
