#include "libuser.h"

static const int tetrominoes[7][4][2] = {
    // I
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
    // O
    {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    // T
    {{0, 1}, {1, 0}, {1, 1}, {1, 2}},
    // S
    {{0, 1}, {0, 2}, {1, 0}, {1, 1}},
    // Z
    {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
    // J
    {{0, 0}, {1, 0}, {1, 1}, {1, 2}},
    // L
    {{0, 2}, {1, 0}, {1, 1}, {1, 2}}};

static char tetris_board[20][10];

static int check_tetris_collision(int cells[4][2], int cx, int cy) {
  for (int i = 0; i < 4; i++) {
    int x = cells[i][0] + cx;
    int y = cells[i][1] + cy;

    if (x < 0 || x >= 10 || y >= 20) {
      return 1;
    }
    if (y >= 0 && tetris_board[y][x] != 0) {
      return 1;
    }
  }
  return 0;
}

static void rotate_tetris_piece(int cells[4][2], int pivot_idx) {
  int px = cells[pivot_idx][0];
  int py = cells[pivot_idx][1];
  for (int i = 0; i < 4; i++) {
    int x = cells[i][0];
    int y = cells[i][1];
    int nx = px - (y - py);
    int ny = py + (x - px);
    cells[i][0] = nx;
    cells[i][1] = ny;
  }
}

static int cur_piece = 0;
static int cur_cells[4][2];
static int cur_x = 3;
static int cur_y = 0;
static int cur_color = 0x0A;
static int tetris_game_over = 0;
static unsigned int tetris_seed = 54321;

static void spawn_piece() {
  tetris_seed = (tetris_seed * 1103515245 + 12345) & 0x7FFFFFFF;
  cur_piece = tetris_seed % 7;
  for (int i = 0; i < 4; i++) {
    cur_cells[i][0] = tetrominoes[cur_piece][i][0];
    cur_cells[i][1] = tetrominoes[cur_piece][i][1];
  }
  cur_x = 3;
  cur_y = 0;
  const int colors[7] = {0x0B, 0x0E, 0x0D, 0x0A, 0x0C, 0x09, 0x0E};
  cur_color = colors[cur_piece];

  if (check_tetris_collision(cur_cells, cur_x, cur_y)) {
    tetris_game_over = 1;
  }
}

static char tetris_scancode_to_ascii(unsigned char scancode) {
  switch (scancode) {
    case 0x10: return 'q';
    case 0x1E: return 'a';
    case 0x20: return 'd';
    case 0x1F: return 's';
    case 0x11: return 'w';
    case 0x24: return 'j';
    case 0x26: return 'l';
    case 0x25: return 'k';
    case 0x17: return 'i';
    default: return 0;
  }
}

void _start() {
  for (int r = 0; r < 20; r++) {
    for (int c = 0; c < 10; c++) {
      tetris_board[r][c] = 0;
    }
  }

  int score = 0;
  tetris_game_over = 0;
  spawn_piece();

  int loop_counter = 0;
  int drop_speed = 30;

  while (!tetris_game_over) {
    u_gfx_clear(0);

    u_gfx_draw_string(215, 30, "TETRIS GAME", 14);
    u_gfx_draw_string(215, 50, "Score:", 15);

    char score_num[10];
    u_itoa(score, score_num);
    u_gfx_draw_string(215, 60, score_num, 10);

    u_gfx_draw_string(215, 90, "Controls:", 15);
    u_gfx_draw_string(215, 105, " A/J - Left", 7);
    u_gfx_draw_string(215, 115, " D/L - Right", 7);
    u_gfx_draw_string(215, 125, " W/I - Rotate", 7);
    u_gfx_draw_string(215, 135, " S/K - Drop", 7);
    u_gfx_draw_string(215, 145, " Q   - Quit", 12);

    u_draw_line(119, 19, 119, 181, 15);
    u_draw_line(201, 19, 201, 181, 15);
    u_draw_line(119, 181, 201, 181, 15);

    for (int r = 0; r < 20; r++) {
      for (int c = 0; c < 10; c++) {
        int val = tetris_board[r][c];
        int bx = 120 + c * 8;
        int by = 20 + r * 8;
        if (val != 0) {
          u_draw_rect_filled(bx, by, 7, 7, val);
          u_draw_rect(bx, by, 8, 8, 0);
        } else {
          u_gfx_draw_pixel(bx + 4, by + 4, 8);
        }
      }
    }

    for (int i = 0; i < 4; i++) {
      int x = cur_cells[i][0] + cur_x;
      int y = cur_cells[i][1] + cur_y;
      if (y >= 0 && y < 20 && x >= 0 && x < 10) {
        int bx = 120 + x * 8;
        int by = 20 + y * 8;
        u_draw_rect_filled(bx, by, 7, 7, cur_color);
        u_draw_rect(bx, by, 8, 8, 0);
      }
    }

    u_gfx_swap();

    for (volatile int d = 0; d < 80000; d++);

    if (u_key_has()) {
      unsigned char scancode = u_key_read();
      if (scancode < 0x80) {
        char ascii = tetris_scancode_to_ascii(scancode);
        if (ascii == 'q' || ascii == 'Q') {
          tetris_game_over = 1;
        } else if (ascii == 'a' || ascii == 'j') {
          if (!check_tetris_collision(cur_cells, cur_x - 1, cur_y)) {
            cur_x--;
          }
        } else if (ascii == 'd' || ascii == 'l') {
          if (!check_tetris_collision(cur_cells, cur_x + 1, cur_y)) {
            cur_x++;
          }
        } else if (ascii == 's' || ascii == 'k') {
          if (!check_tetris_collision(cur_cells, cur_x, cur_y + 1)) {
            cur_y++;
          }
        } else if (ascii == 'w' || ascii == 'i') {
          if (cur_piece != 1) {
            int next[4][2];
            for (int k = 0; k < 4; k++) {
              next[k][0] = cur_cells[k][0];
              next[k][1] = cur_cells[k][1];
            }
            rotate_tetris_piece(next, 1);
            if (!check_tetris_collision(next, cur_x, cur_y)) {
              for (int k = 0; k < 4; k++) {
                cur_cells[k][0] = next[k][0];
                cur_cells[k][1] = next[k][1];
              }
            }
          }
        }
      }
      for (volatile int d = 0; d < 400000; d++);
    }

    loop_counter++;
    if (loop_counter >= drop_speed) {
      loop_counter = 0;
      if (!check_tetris_collision(cur_cells, cur_x, cur_y + 1)) {
        cur_y++;
      } else {
        for (int i = 0; i < 4; i++) {
          int x = cur_cells[i][0] + cur_x;
          int y = cur_cells[i][1] + cur_y;
          if (y >= 0 && y < 20 && x >= 0 && x < 10) {
            tetris_board[y][x] = cur_color;
          }
        }

        for (int r = 19; r >= 0; r--) {
          int full = 1;
          for (int c = 0; c < 10; c++) {
            if (tetris_board[r][c] == 0) {
              full = 0;
              break;
            }
          }
          if (full) {
            score += 10;
            for (int shift_r = r; shift_r > 0; shift_r--) {
              for (int c = 0; c < 10; c++) {
                tetris_board[shift_r][c] = tetris_board[shift_r - 1][c];
              }
            }
            for (int c = 0; c < 10; c++) {
              tetris_board[0][c] = 0;
            }
            r++;
          }
        }

        spawn_piece();
      }
    }
  }

  u_gfx_clear(0);
  u_draw_rect_filled(50, 60, 220, 80, 12);
  u_draw_rect(48, 58, 224, 84, 15);

  u_gfx_draw_string(120, 75, "GAME OVER", 15);

  char score_msg[] = "Final Score: ";
  u_gfx_draw_string(90, 100, score_msg, 15);
  char score_num[10];
  u_itoa(score, score_num);
  u_gfx_draw_string(190, 100, score_num, 14);

  u_gfx_draw_string(70, 120, "Press any key to exit", 10);
  u_gfx_swap();

  for (volatile int d = 0; d < 800000; d++);
  while (u_key_has()) {
    u_key_read();
  }

  while (1) {
    if (u_key_has()) {
      unsigned char scancode = u_key_read();
      if (scancode < 0x80) {
        break;
      }
    }
  }
  u_exit();
}
