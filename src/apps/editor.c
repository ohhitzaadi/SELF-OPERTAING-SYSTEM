#include "libuser.h"

#define TEXT_BUFFER_MAX 1024
static char text_buffer[TEXT_BUFFER_MAX];
static int text_len = 0;
static int shift_state = 0;

static char scancode_to_ascii(unsigned char scancode, int shift) {
  if (!shift) {
    switch (scancode) {
      case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3'; case 0x05: return '4';
      case 0x06: return '5'; case 0x07: return '6'; case 0x08: return '7'; case 0x09: return '8';
      case 0x0A: return '9'; case 0x0B: return '0'; case 0x0C: return '-'; case 0x0D: return '=';
      case 0x0E: return '\b'; case 0x0F: return '\t'; case 0x10: return 'q'; case 0x11: return 'w';
      case 0x12: return 'e'; case 0x13: return 'r'; case 0x14: return 't'; case 0x15: return 'y';
      case 0x16: return 'u'; case 0x17: return 'i'; case 0x18: return 'o'; case 0x19: return 'p';
      case 0x1A: return '['; case 0x1B: return ']'; case 0x1C: return '\n'; case 0x1E: return 'a';
      case 0x1F: return 's'; case 0x20: return 'd'; case 0x21: return 'f'; case 0x22: return 'g';
      case 0x23: return 'h'; case 0x24: return 'j'; case 0x25: return 'k'; case 0x26: return 'l';
      case 0x27: return ';'; case 0x28: return '\''; case 0x29: return '`'; case 0x2B: return '\\';
      case 0x2C: return 'z'; case 0x2D: return 'x'; case 0x2E: return 'c'; case 0x2F: return 'v';
      case 0x30: return 'b'; case 0x31: return 'n'; case 0x32: return 'm'; case 0x33: return ',';
      case 0x34: return '.'; case 0x35: return '/'; case 0x39: return ' ';
      default: return 0;
    }
  } else {
    switch (scancode) {
      case 0x02: return '!'; case 0x03: return '@'; case 0x04: return '#'; case 0x05: return '$';
      case 0x06: return '%'; case 0x07: return '^'; case 0x08: return '&'; case 0x09: return '*';
      case 0x0A: return '('; case 0x0B: return ')'; case 0x0C: return '_'; case 0x0D: return '+';
      case 0x0E: return '\b'; case 0x0F: return '\t'; case 0x10: return 'Q'; case 0x11: return 'W';
      case 0x12: return 'E'; case 0x13: return 'R'; case 0x14: return 'T'; case 0x15: return 'Y';
      case 0x16: return 'U'; case 0x17: return 'I'; case 0x18: return 'O'; case 0x19: return 'P';
      case 0x1A: return '{'; case 0x1B: return '}'; case 0x1C: return '\n'; case 0x1E: return 'A';
      case 0x1F: return 'S'; case 0x20: return 'D'; case 0x21: return 'F'; case 0x22: return 'G';
      case 0x23: return 'H'; case 0x24: return 'J'; case 0x25: return 'K'; case 0x26: return 'L';
      case 0x27: return ':'; case 0x28: return '"'; case 0x29: return '~'; case 0x2B: return '|';
      case 0x2C: return 'Z'; case 0x2D: return 'X'; case 0x2E: return 'C'; case 0x2F: return 'V';
      case 0x30: return 'B'; case 0x31: return 'N'; case 0x32: return 'M'; case 0x33: return '<';
      case 0x34: return '>'; case 0x35: return '?'; case 0x39: return ' ';
      default: return 0;
    }
  }
}

static void draw_editor_text(int *cursor_x_out, int *cursor_y_out) {
  int cx = 10;
  int cy = 22;
  
  for (int i = 0; i < text_len; i++) {
    char c = text_buffer[i];
    if (c == '\n') {
      cx = 10;
      cy += 9;
    } else {
      u_gfx_draw_char(cx, cy, c, 0); // Black text on white canvas
      cx += 6;
      if (cx >= 310) {
        cx = 10;
        cy += 9;
      }
    }
    // Limit displaying to canvas height
    if (cy >= 165) break;
  }
  
  *cursor_x_out = cx;
  *cursor_y_out = cy;
}

static void draw_ui(int cx, int cy) {
  // 1. Title bar (gray background)
  u_draw_rect_filled(0, 0, 320, 15, 7);
  u_gfx_draw_string(10, 3, "NEXUS TEXT EDITOR GUI", 0);
  u_draw_line(0, 14, 320, 14, 8);
  
  // 2. Main text editing canvas (white background)
  u_draw_line(0, 175, 320, 175, 8);
  
  // 3. Blinking cursor
  if (((u_get_ticks() / 15) & 1) == 0) {
    u_gfx_draw_char(cx, cy, '_', 0); // Blinking black cursor
  }
  
  // 4. Command toolbar at the bottom panel
  u_draw_rect_filled(0, 176, 320, 24, 7);
  
  // [New]
  u_draw_rect_filled(10, 180, 32, 10, 8);
  u_gfx_draw_string(13, 181, "New", 15);
  
  // [Save]
  u_draw_rect_filled(48, 180, 32, 10, 8);
  u_gfx_draw_string(51, 181, "Save", 15);
  
  // [Load]
  u_draw_rect_filled(86, 180, 32, 10, 8);
  u_gfx_draw_string(89, 181, "Load", 15);
  
  // [Exit]
  u_draw_rect_filled(124, 180, 32, 10, 8);
  u_gfx_draw_string(127, 181, "Exit", 15);
  
  // Helper shortcuts text
  u_gfx_draw_string(164, 181, "Esc/Q to Quit", 0);
}

void _start() {
  text_buffer[0] = '\0';
  text_len = 0;
  shift_state = 0;
  
  u_gfx_clear(0);
  
  int running = 1;
  int status_timer = 0;
  char status_msg[32] = "";
  
  while (running) {
    int cur_cx = 0, cur_cy = 0;
    
    // 1. Draw UI and Text
    u_draw_rect_filled(0, 15, 320, 160, 15); // White canvas redraw
    draw_editor_text(&cur_cx, &cur_cy);
    draw_ui(cur_cx, cur_cy);
    
    // Status text if active
    if (status_timer > 0) {
      u_gfx_draw_string(230, 3, status_msg, 14); // Yellow notification
      status_timer--;
    }
    
    // 2. Mouse Click Handling
    int clicks, mx, my;
    u_mouse_get(&clicks, &mx, &my);
    
    if (clicks & 1) { // Left mouse pressed
      if (my >= 176 && my <= 190) {
        // [New]
        if (mx >= 10 && mx < 42) {
          text_buffer[0] = '\0';
          text_len = 0;
          u_strcpy(status_msg, "Cleared!");
          status_timer = 30;
        }
        else if (mx >= 48 && mx < 80) {
          // Save directly to persistent storage
          u_fs_write("NOTE.TXT", text_buffer);
          u_strcpy(status_msg, "Saved!");
          status_timer = 30;
        }
        // [Load]
        else if (mx >= 86 && mx < 118) {
          // Load directly from persistent storage
          int size = u_fs_read("NOTE.TXT", (unsigned char *)text_buffer);
          if (size >= 0) {
            text_len = size;
            text_buffer[text_len] = '\0';
            u_strcpy(status_msg, "Loaded!");
          } else {
            u_strcpy(status_msg, "No Save found!");
          }
          status_timer = 30;
        }
        // [Exit]
        else if (mx >= 124 && mx < 156) {
          running = 0;
        }
      }
    }
    
    // Draw mouse crosshair
    u_gfx_draw_pixel(mx, my, 14);
    u_draw_line(mx - 3, my, mx + 3, my, 14);
    u_draw_line(mx, my - 3, mx, my + 3, 14);
    
    u_gfx_swap();
    
    // 3. Keyboard Input processing
    while (u_key_has()) {
      unsigned char scancode = u_key_read();
      
      // Shift keys make/break detection
      if (scancode == 0x2A || scancode == 0x36) {
        shift_state = 1;
      } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_state = 0;
      }
      
      if (scancode < 128) {
        if (scancode == 0x01) { // Esc key exits
          running = 0;
        } else if (scancode == 0x0E) { // Direct Backspace raw scan code check
          if (text_len > 0) {
            text_len--;
            text_buffer[text_len] = '\0';
          }
        } else {
          char ascii = scancode_to_ascii(scancode, shift_state);
          if (ascii != 0 && ascii != '\b') {
            if (text_len < TEXT_BUFFER_MAX - 1) {
              text_buffer[text_len++] = ascii;
              text_buffer[text_len] = '\0';
            }
          }
        }
      }
    }
    
    u_yield();
  }
  
  // Clear keyboard buffer before exiting
  while (u_key_has()) {
    u_key_read();
  }
  
  u_exit();
}
