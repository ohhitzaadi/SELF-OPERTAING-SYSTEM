#include "libuser.h"

// Canvas size: 160x80 pixels.
// Drawn scaled 2x in the screen region: X from 0 to 319, Y from 15 to 174.
#define CANVAS_W 160
#define CANVAS_H 80
#define CANVAS_OFFSET_Y 15

static unsigned char canvas[CANVAS_H][CANVAS_W];
static unsigned char selected_color = 0; // Black default
static int brush_size = 1; // 1, 2, or 3

// 12 Palette colors mapping
static unsigned char palette_colors[12] = {
  0,  // Black
  1,  // Blue
  2,  // Green
  3,  // Cyan
  4,  // Red
  5,  // Magenta
  20, // Dark Gray
  24, // Gray
  9,  // Light Blue
  10, // Light Green
  12, // Light Red
  14  // Yellow
};

static void init_canvas() {
  for (int y = 0; y < CANVAS_H; y++) {
    for (int x = 0; x < CANVAS_W; x++) {
      canvas[y][x] = 15; // White background
    }
  }
}

static void draw_canvas() {
  // Render each canvas pixel as a 2x2 screen block
  for (int y = 0; y < CANVAS_H; y++) {
    int sy = y * 2 + CANVAS_OFFSET_Y;
    for (int x = 0; x < CANVAS_W; x++) {
      int sx = x * 2;
      unsigned char col = canvas[y][x];
      backbuffer[sy * 320 + sx] = col;
      backbuffer[sy * 320 + (sx + 1)] = col;
      backbuffer[(sy + 1) * 320 + sx] = col;
      backbuffer[(sy + 1) * 320 + (sx + 1)] = col;
    }
  }
}

static void draw_ui() {
  // 1. Draw top header (gray background)
  u_draw_rect_filled(0, 0, 320, CANVAS_OFFSET_Y, 7); // Light gray panel
  u_gfx_draw_string(10, 3, "NEXUS MS-PAINT GUI", 0);  // Black text
  u_draw_line(0, CANVAS_OFFSET_Y - 1, 320, CANVAS_OFFSET_Y - 1, 8); // Dark gray border
  
  // 2. Draw bottom panel background
  u_draw_rect_filled(0, 175, 320, 25, 7);
  u_draw_line(0, 175, 320, 175, 8);
  
  // 3. Draw command buttons
  // [Clear]
  u_draw_rect_filled(10, 178, 38, 10, 8);
  u_gfx_draw_string(13, 179, "Clear", 15);
  
  // [Save]
  u_draw_rect_filled(52, 178, 32, 10, 8);
  u_gfx_draw_string(55, 179, "Save", 15);
  
  // [Load]
  u_draw_rect_filled(88, 178, 32, 10, 8);
  u_gfx_draw_string(91, 179, "Load", 15);
  
  // [Exit]
  u_draw_rect_filled(124, 178, 32, 10, 8);
  u_gfx_draw_string(127, 179, "Exit", 15);
  
  // 4. Draw brush selection buttons
  u_gfx_draw_string(162, 179, "Sz:", 0);
  
  // [1]
  u_draw_rect_filled(182, 178, 10, 10, (brush_size == 1) ? 14 : 8);
  u_gfx_draw_string(185, 179, "1", (brush_size == 1) ? 0 : 15);
  
  // [2]
  u_draw_rect_filled(196, 178, 10, 10, (brush_size == 2) ? 14 : 8);
  u_gfx_draw_string(199, 179, "2", (brush_size == 2) ? 0 : 15);
  
  // [3]
  u_draw_rect_filled(210, 178, 10, 10, (brush_size == 3) ? 14 : 8);
  u_gfx_draw_string(213, 179, "3", (brush_size == 3) ? 0 : 15);
  
  // 5. Draw active color box
  u_gfx_draw_string(228, 179, "Col:", 0);
  u_draw_rect_filled(252, 178, 10, 10, selected_color);
  u_draw_rect(252, 178, 10, 10, 0); // Black outline
  
  // 6. Draw color palette
  u_gfx_draw_string(10, 191, "Palette:", 0);
  for (int i = 0; i < 12; i++) {
    int px = 62 + i * 14;
    u_draw_rect_filled(px, 190, 10, 8, palette_colors[i]);
    u_draw_rect(px, 190, 10, 8, 0);
  }
}

static void draw_brush_stroke(int cx, int cy) {
  int radius = brush_size - 1;
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      int nx = cx + dx;
      int ny = cy + dy;
      if (nx >= 0 && nx < CANVAS_W && ny >= 0 && ny < CANVAS_H) {
        canvas[ny][nx] = selected_color;
      }
    }
  }
}

void _start() {
  init_canvas();
  u_gfx_clear(0);
  
  int running = 1;
  int status_timer = 0;
  char status_msg[32] = "";
  
  while (running) {
    // 1. Draw canvas pixels and UI components
    draw_canvas();
    draw_ui();
    
    // Status text if active
    if (status_timer > 0) {
      u_gfx_draw_string(230, 3, status_msg, 14); // Yellow notification text
      status_timer--;
    }
    
    // 2. Mouse Input polling
    int clicks, mx, my;
    u_mouse_get(&clicks, &mx, &my);
    
    // Handle click interactions
    if (clicks & 1) { // Left-click pressed
      if (my >= 15 && my < 175) {
        // Draw inside canvas boundary
        int cx = mx / 2;
        int cy = (my - 15) / 2;
        draw_brush_stroke(cx, cy);
      } else if (my >= 178 && my <= 188) {
        // Check Command Buttons
        if (mx >= 10 && mx < 48) {
          init_canvas();
          u_strcpy(status_msg, "Cleared!");
          status_timer = 30;
        } else if (mx >= 52 && mx < 84) {
          // Save canvas data directly to the hard disk
          u_fs_write("PAINT.RAW", (const char *)canvas);
          u_strcpy(status_msg, "Saved!");
          status_timer = 30;
        } else if (mx >= 88 && mx < 120) {
          // Load canvas data directly from the hard disk
          int size = u_fs_read("PAINT.RAW", (unsigned char *)canvas);
          if (size >= 0) {
            u_strcpy(status_msg, "Loaded!");
          } else {
            u_strcpy(status_msg, "No Save found!");
          }
          status_timer = 30;
        } else if (mx >= 124 && mx < 156) {
          running = 0;
        }
        // Check Brush Size Buttons
        else if (mx >= 182 && mx < 192) {
          brush_size = 1;
        } else if (mx >= 196 && mx < 206) {
          brush_size = 2;
        } else if (mx >= 210 && mx < 220) {
          brush_size = 3;
        }
      } else if (my >= 190 && my <= 198) {
        // Select Color from Palette
        int idx = (mx - 62) / 14;
        if (idx >= 0 && idx < 12) {
          selected_color = palette_colors[idx];
        }
      }
    }
    
    // Draw cursor crosshair at current position
    u_gfx_draw_pixel(mx, my, 14); // Central yellow pixel
    u_draw_line(mx - 3, my, mx + 3, my, 14);
    u_draw_line(mx, my - 3, mx, my + 3, 14);
    
    u_gfx_swap();
    
    // 3. Keyboard Input checking
    while (u_key_has()) {
      unsigned char scancode = u_key_read();
      if (scancode == 0x01 || scancode == 0x10) { // Esc or Q
        running = 0;
      }
    }
    
    u_yield();
  }
  
  // Clean keyboard buffer before exiting
  while (u_key_has()) {
    u_key_read();
  }
  
  u_exit();
}
