#include "libuser.h"

#define PI 3.14159265f

static float clamp_angle(float a) {
  while (a < -PI) a += 2.0f * PI;
  while (a > PI) a -= 2.0f * PI;
  return a;
}

static float ksin(float x) {
  x = clamp_angle(x);
  float x2 = x * x;
  return x * (1.0f - x2 * (0.166666667f - x2 * (0.008333333f - x2 * 0.000198412f)));
}

static float kcos(float x) {
  return ksin(x + PI / 2.0f);
}

typedef struct {
  float x;
  float y;
  int health;
  int active;
} enemy_t;

#define NUM_ENEMIES 3

void _start() {
  // Bigger 16x16 Map
  int map[16][16] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 2, 2, 0, 3, 0, 1, 0, 2, 2, 0, 3, 3, 0, 1},
      {1, 0, 2, 0, 0, 3, 0, 1, 0, 2, 0, 0, 0, 3, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 3, 0, 2, 2, 0, 1, 0, 3, 0, 2, 2, 2, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 2, 0, 2, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 1},
      {1, 0, 2, 0, 2, 0, 0, 1, 0, 0, 2, 0, 2, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 3, 3, 3, 0, 0, 1, 0, 0, 3, 3, 3, 3, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
  };

  // Four spawn locations in the corners of the map
  int enemy_spawn_points[4][2] = {
      {1, 1},
      {14, 1},
      {1, 14},
      {14, 14}
  };
  int current_spawn_idx = 0;

  // Initialize multiple enemies
  enemy_t enemies[NUM_ENEMIES];
  enemies[0].x = 1.5f;   enemies[0].y = 1.5f;   enemies[0].health = 3; enemies[0].active = 1;
  enemies[1].x = 14.5f;  enemies[1].y = 1.5f;   enemies[1].health = 3; enemies[1].active = 1;
  enemies[2].x = 1.5f;   enemies[2].y = 14.5f;  enemies[2].health = 3; enemies[2].active = 1;

  for (int i = 0; i < NUM_ENEMIES; i++) {
    map[(int)enemies[i].y][(int)enemies[i].x] = 4;
  }

  // Player starts in the center room
  float px = 8.5f;
  float py = 8.5f;
  float pa = 0.0f;
  float fov = 1.047f; // 60 degrees

  int last_mx = -1;
  int last_click_l = 0;
  int score = 0;
  int player_hp = 100;
  int reload_timer = 0;

  // Track keyboard states for continuous movement
  int key_states[128];
  for (int i = 0; i < 128; i++) {
    key_states[i] = 0;
  }

  int running = 1;
  while (running) {
    // Record current tick count at frame start
    unsigned int start_ticks = u_get_ticks();

    // 1. Game Over Check
    if (player_hp <= 0) {
      u_gfx_clear(0);
      u_gfx_draw_string(110, 80, "GAME OVER", 12); // Red text
      u_gfx_draw_string(80, 100, "Press any key to exit", 15); // White text
      
      char score_str[40];
      u_strcpy(score_str, "Final Score: ");
      char num[10];
      u_itoa(score, num);
      u_strcpy(score_str + u_strlen(score_str), num);
      u_gfx_draw_string(95, 120, score_str, 14); // Yellow text
      
      u_gfx_swap();
      
      // Wait for keypress
      while (!u_key_has()) {
        for (volatile int d = 0; d < 10000; d++);
      }
      u_key_read();
      running = 0;
      break;
    }

    // 2. Read Keyboard Inputs and Update Key States (Press / Release)
    while (u_key_has()) {
      unsigned char scancode = u_key_read();
      if (scancode < 128) {
        key_states[scancode] = 1; // Key Pressed
      } else if (scancode >= 0x80 && (scancode & 0x7F) < 128) {
        key_states[scancode & 0x7F] = 0; // Key Released
      }
    }

    // 3. Read Mouse Inputs
    int clicks, mx, my;
    u_mouse_get(&clicks, &mx, &my);
    if (last_mx == -1) {
      last_mx = mx;
    }

    // Mouse Look (horizontal delta)
    int diff_x = mx - last_mx;
    if (diff_x < -100) diff_x = -100;
    if (diff_x > 100) diff_x = 100;
    pa += (float)diff_x * 0.005f;
    pa = clamp_angle(pa);
    last_mx = mx;

    // Process shooting flag
    int fire_weapon = 0;

    // Continuous Keyboard Movement and Actions
    if (key_states[0x11]) { // 'W' - Forward
      float nx = px + kcos(pa) * 0.04f;
      float ny = py + ksin(pa) * 0.04f;
      if (map[(int)ny][(int)nx] == 0) {
        px = nx;
        py = ny;
      }
    }
    if (key_states[0x1F]) { // 'S' - Backward
      float nx = px - kcos(pa) * 0.04f;
      float ny = py - ksin(pa) * 0.04f;
      if (map[(int)ny][(int)nx] == 0) {
        px = nx;
        py = ny;
      }
    }
    if (key_states[0x1E]) { // 'A' - Rotate Left
      pa -= 0.04f;
      pa = clamp_angle(pa);
    }
    if (key_states[0x20]) { // 'D' - Rotate Right
      pa += 0.04f;
      pa = clamp_angle(pa);
    }
    if (key_states[0x39]) { // Spacebar - Shoot
      fire_weapon = 1;
      key_states[0x39] = 0; // Require re-press
    }
    if (key_states[0x10]) { // 'Q' - Quit
      running = 0;
    }

    // Detect Mouse Left Click shooting trigger
    int click_l = clicks & 1;
    if (click_l && !last_click_l) {
      fire_weapon = 1;
    }
    last_click_l = click_l;

    // 4. Update Active Enemies AI & Collision (run before raycasting)
    for (int i = 0; i < NUM_ENEMIES; i++) {
      if (!enemies[i].active) continue;

      float ex = enemies[i].x;
      float ey = enemies[i].y;

      // Distance to player
      float dx = px - ex;
      float dy = py - ey;
      float dist_sq = dx * dx + dy * dy;

      // Clear old cell from map
      map[(int)ey][(int)ex] = 0;

      // Chase player if in range (chase threshold = 7.0 units)
      if (dist_sq < 49.0f) {
        // Player contact damage
        if (dist_sq < 0.2f) {
          player_hp--;
        }

        float step_x = (dx > 0.0f) ? 0.015f : -0.015f;
        float step_y = (dy > 0.0f) ? 0.015f : -0.015f;

        float nx = ex + step_x;
        float ny = ey + step_y;

        // Check map limits & grid collision (ignore other enemies/block type 4 for movement)
        int g_nx = (int)nx;
        int g_ny = (int)ey;
        if (g_nx >= 0 && g_nx < 16 && g_ny >= 0 && g_ny < 16) {
          int cell = map[g_ny][g_nx];
          if (cell == 0 || cell == 4) {
            ex = nx;
          }
        }

        g_nx = (int)ex;
        g_ny = (int)ny;
        if (g_nx >= 0 && g_nx < 16 && g_ny >= 0 && g_ny < 16) {
          int cell = map[g_ny][g_nx];
          if (cell == 0 || cell == 4) {
            ey = ny;
          }
        }

        enemies[i].x = ex;
        enemies[i].y = ey;
      }

      // Restore cell on map
      map[(int)enemies[i].y][(int)enemies[i].x] = 4;
    }

    // 5. Draw Sky (top half) and Floor (bottom half)
    for (int y = 0; y < 100; y++) {
      for (int x = 0; x < 320; x++) {
        backbuffer[y * 320 + x] = 104; // Dark blue sky
      }
    }
    for (int y = 100; y < 200; y++) {
      for (int x = 0; x < 320; x++) {
        backbuffer[y * 320 + x] = 23; // Dark grey floor
      }
    }

    // 6. Cast 320 Rays (limit distance to 16.0f for larger map)
    int enemy_in_crosshair = 0;
    float enemy_dist = 999.0f;
    float enemy_hit_x = 0.0f;
    float enemy_hit_y = 0.0f;

    for (int x = 0; x < 320; x++) {
      float ray_angle = pa - (fov / 2.0f) + ((float)x / 320.0f) * fov;
      float rx = px;
      float ry = py;
      float step = 0.04f;
      float dist = 0.0f;
      int hit = 0;

      while (dist < 16.0f) {
        rx += kcos(ray_angle) * step;
        ry += ksin(ray_angle) * step;
        dist += step;

        int mx_map = (int)rx;
        int my_map = (int)ry;
        if (mx_map >= 0 && mx_map < 16 && my_map >= 0 && my_map < 16 && map[my_map][mx_map] != 0) {
          hit = map[my_map][mx_map];
          break;
        }
      }

      // Check if center ray hits the enemy
      if (x == 160 && hit == 4) {
        enemy_in_crosshair = 1;
        enemy_dist = dist;
        enemy_hit_x = rx;
        enemy_hit_y = ry;
      }

      float corrected_dist = dist * kcos(ray_angle - pa);
      if (corrected_dist < 0.1f) corrected_dist = 0.1f;

      int wall_h = (int)(150.0f / corrected_dist);
      if (wall_h > 180) wall_h = 180;

      int draw_start = 100 - wall_h / 2;
      int draw_end = 100 + wall_h / 2;

      unsigned char wall_color = 11; // Cyan
      if (hit == 2) wall_color = 12; // Red
      if (hit == 3) wall_color = 14; // Yellow
      if (hit == 4) wall_color = 13; // Magenta (Enemy target)

      // Apply simple depth shading
      if (dist > 6.0f) {
        if (wall_color == 11) wall_color = 3;
        else if (wall_color == 12) wall_color = 4;
        else if (wall_color == 14) wall_color = 6;
        else if (wall_color == 13) wall_color = 5; // Dark Magenta
      }

      for (int y = draw_start; y < draw_end; y++) {
        backbuffer[y * 320 + x] = wall_color;
      }
    }

    // 7. Weapon Firing & Reload State Machine Logic
    int gun_y_offset = 0;
    int show_flash = 0;

    if (reload_timer > 0) {
      if (reload_timer >= 10) {
        gun_y_offset = 8;
        show_flash = 1;
      } else if (reload_timer >= 6) {
        gun_y_offset = 24; // Lowered / Pump state
      } else {
        gun_y_offset = 8; // Raising state
      }
      reload_timer--;
    }

    if (fire_weapon && reload_timer == 0) {
      reload_timer = 12; // Start reload pump animation cycle

      // Hit confirmation check
      if (enemy_in_crosshair && enemy_dist < 8.0f) {
        // Resolve closest enemy to the raycast coordinates (enemy_hit_x, enemy_hit_y)
        int hit_idx = -1;
        float min_dist_sq = 999.0f;
        for (int e = 0; e < NUM_ENEMIES; e++) {
          if (!enemies[e].active) continue;
          float dx = enemies[e].x - enemy_hit_x;
          float dy = enemies[e].y - enemy_hit_y;
          float d_sq = dx*dx + dy*dy;
          if (d_sq < min_dist_sq) {
            min_dist_sq = d_sq;
            hit_idx = e;
          }
        }

        if (hit_idx != -1 && min_dist_sq < 2.0f) {
          enemies[hit_idx].health--;
          if (enemies[hit_idx].health <= 0) {
            score += 100;
            enemies[hit_idx].active = 0;
            map[(int)enemies[hit_idx].y][(int)enemies[hit_idx].x] = 0;

            // Instantly respawn enemy at a vacant spawn point
            for (int s = 0; s < 4; s++) {
              current_spawn_idx = (current_spawn_idx + 1) % 4;
              int sx = enemy_spawn_points[current_spawn_idx][0];
              int sy = enemy_spawn_points[current_spawn_idx][1];
              if (map[sy][sx] == 0) {
                enemies[hit_idx].x = (float)sx + 0.5f;
                enemies[hit_idx].y = (float)sy + 0.5f;
                enemies[hit_idx].health = 3;
                enemies[hit_idx].active = 1;
                map[sy][sx] = 4;
                break;
              }
            }
          }
        }
      }
    }

    // 8. Draw Shotgun with recoil offset
    u_draw_rect_filled(145, 140 + gun_y_offset, 30, 60, 8); // Gun Body
    u_draw_rect_filled(150, 115 + gun_y_offset, 20, 30, 7); // Barrels
    u_draw_line(160, 115 + gun_y_offset, 160, 145 + gun_y_offset, 0); // Dual barrel split line

    // Muzzle Flash
    if (show_flash) {
      u_draw_rect_filled(140, 100 + gun_y_offset, 40, 15, 14); // Yellow outer flash
      u_draw_rect_filled(148, 105 + gun_y_offset, 24, 10, 15); // White inner flash
    }

    // Pump action text display
    if (reload_timer >= 6 && reload_timer < 10) {
      u_gfx_draw_string(142, 130, "*chk-chk*", 10);
    }

    // 9. Render dynamic Crosshair
    unsigned char cross_color = 10; // Green default
    if (enemy_in_crosshair && enemy_dist < 8.0f) {
      cross_color = 12; // Red when locked on enemy target
    }
    u_gfx_draw_pixel(160, 100, cross_color);
    u_draw_line(155, 100, 157, 100, cross_color);
    u_draw_line(163, 100, 165, 100, cross_color);
    u_draw_line(160, 95, 160, 97, cross_color);
    u_draw_line(160, 103, 160, 105, cross_color);

    // Hit confirmation splash
    if (show_flash && enemy_in_crosshair && enemy_dist < 8.0f) {
      u_gfx_draw_string(145, 80, "HIT!", 12);
    }

    // 10. Render Mini-Map in top-left (scaled 16x16)
    for (int my = 0; my < 16; my++) {
      for (int mx_map = 0; mx_map < 16; mx_map++) {
        unsigned char col = 0;
        if (map[my][mx_map] != 0) {
          if (map[my][mx_map] == 4) {
            col = 13; // Magenta dot for enemy
          } else {
            col = 7;  // Grey wall
          }
        }
        u_draw_rect_filled(4 + mx_map * 3, 4 + my * 3, 2, 2, col);
      }
    }
    // Draw player position dot in mini-map
    u_gfx_draw_pixel(4 + (int)(px * 3.0f), 4 + (int)(py * 3.0f), 12); // Red player dot

    // 11. Render HUD texts
    char hud_buf[80];
    u_strcpy(hud_buf, "HP: ");
    char num_buf[10];
    u_itoa(player_hp, num_buf);
    u_strcpy(hud_buf + u_strlen(hud_buf), num_buf);
    u_strcpy(hud_buf + u_strlen(hud_buf), "%   AMMO: INF   SCORE: ");
    u_itoa(score, num_buf);
    u_strcpy(hud_buf + u_strlen(hud_buf), num_buf);
    u_gfx_draw_string(10, 185, hud_buf, 12);

    u_gfx_draw_string(200, 185, "MOUSE - Look/Shoot  Q - Quit", 10);
    u_gfx_draw_string(100, 5, "NEXUS-DOOM 3D ENGINE", 14);

    u_gfx_swap();

    // 12. Regulate frame rate to ~33 FPS using hardware system timer (1 tick = 10ms, wait for 3 ticks)
    while (u_get_ticks() - start_ticks < 3) {
      u_yield();
    }
  }

  u_exit();
}
