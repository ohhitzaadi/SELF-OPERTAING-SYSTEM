#include "libuser.h"

#define PI 3.14159265f

// Custom square root using Newton-Raphson method
static float ksqrt(float x) {
  if (x <= 0.0f) return 0.0f;
  float val = x;
  for (int i = 0; i < 6; i++) {
    val = 0.5f * (val + x / val);
  }
  return val;
}

typedef struct {
  float x, y, z;
} vec3_t;

static vec3_t vec_add(vec3_t a, vec3_t b) {
  return (vec3_t){a.x + b.x, a.y + b.y, a.z + b.z};
}

static vec3_t vec_sub(vec3_t a, vec3_t b) {
  return (vec3_t){a.x - b.x, a.y - b.y, a.z - b.z};
}

static vec3_t vec_mul(vec3_t a, float s) {
  return (vec3_t){a.x * s, a.y * s, a.z * s};
}

static float vec_dot(vec3_t a, vec3_t b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vec3_t vec_normalize(vec3_t a) {
  float len = ksqrt(a.x * a.x + a.y * a.y + a.z * a.z);
  if (len > 0.0f) {
    return (vec3_t){a.x / len, a.y / len, a.z / len};
  }
  return (vec3_t){0, 0, 0};
}

typedef struct {
  vec3_t center;
  float radius;
  int color_type; // 1 = Red, 2 = Green, 3 = Mirror
} sphere_t;

#define NUM_SPHERES 3
static sphere_t spheres[NUM_SPHERES];

static vec3_t cam_pos = {0.0f, 0.0f, 0.0f};
static vec3_t light_pos = {0.0f, 1.8f, 3.0f};

// Ray-Sphere intersection helper
static int intersect_sphere(vec3_t o, vec3_t d, sphere_t s, float *t_out) {
  vec3_t v = vec_sub(o, s.center);
  float b = 2.0f * vec_dot(d, v);
  float c = vec_dot(v, v) - s.radius * s.radius;
  float disc = b * b - 4.0f * c;
  if (disc < 0.0f) return 0;
  
  float disc_sqrt = ksqrt(disc);
  float t1 = (-b - disc_sqrt) / 2.0f;
  float t2 = (-b + disc_sqrt) / 2.0f;
  
  if (t1 > 0.001f) {
    *t_out = t1;
    return 1;
  }
  if (t2 > 0.001f) {
    *t_out = t2;
    return 1;
  }
  return 0;
}

// Ray-Floor plane intersection helper (Plane at Y = -1.0)
static int intersect_floor(vec3_t o, vec3_t d, float *t_out) {
  if (d.y > -0.0001f && d.y < 0.0001f) return 0;
  float t = (-1.0f - o.y) / d.y;
  if (t > 0.001f) {
    *t_out = t;
    return 1;
  }
  return 0;
}

// Trace a single ray and return the VGA palette index
static unsigned char trace(vec3_t o, vec3_t d, int depth) {
  float closest_t = 9999.0f;
  int hit_type = 0; // 0 = Sky, 1 = Sphere 0, 2 = Sphere 1, 3 = Sphere 2, 4 = Floor
  int sphere_idx = -1;
  
  // 1. Check intersections with spheres
  for (int i = 0; i < NUM_SPHERES; i++) {
    float t;
    if (intersect_sphere(o, d, spheres[i], &t)) {
      if (t < closest_t) {
        closest_t = t;
        hit_type = 1 + i;
        sphere_idx = i;
      }
    }
  }
  
  // 2. Check intersection with floor plane
  float t_floor;
  if (intersect_floor(o, d, &t_floor)) {
    if (t_floor < closest_t) {
      closest_t = t_floor;
      hit_type = 4;
    }
  }
  
  // 3. Render Sky if no hit
  if (hit_type == 0) {
    // Beautiful sky gradient from index 104 (dark blue) to 111 (lighter blue)
    int grad = (int)(d.y * 12.0f);
    if (grad < 0) grad = 0;
    if (grad > 7) grad = 7;
    return 104 + grad;
  }
  
  // Compute hit point and normal
  vec3_t hit_pos = vec_add(o, vec_mul(d, closest_t));
  vec3_t normal = {0, 0, 0};
  
  if (hit_type >= 1 && hit_type <= 3) {
    // Sphere normal
    normal = vec_normalize(vec_sub(hit_pos, spheres[sphere_idx].center));
  } else if (hit_type == 4) {
    // Floor normal
    normal = (vec3_t){0.0f, 1.0f, 0.0f};
  }
  
  // 4. Handle Mirror reflection recursively
  if (hit_type >= 1 && hit_type <= 3 && spheres[sphere_idx].color_type == 3) {
    if (depth < 1) {
      // Reflection direction: R = D - 2 * (D . N) * N
      float dot_dn = vec_dot(d, normal);
      vec3_t r_dir = vec_normalize(vec_sub(d, vec_mul(normal, 2.0f * dot_dn)));
      // Offset origin to prevent self-intersection
      vec3_t r_orig = vec_add(hit_pos, vec_mul(normal, 0.005f));
      return trace(r_orig, r_dir, depth + 1);
    } else {
      return 104; // Return sky color if max recursion reached
    }
  }
  
  // 5. Lighting Calculation
  vec3_t light_dir = vec_normalize(vec_sub(light_pos, hit_pos));
  float diffuse = vec_dot(normal, light_dir);
  if (diffuse < 0.0f) diffuse = 0.0f;
  
  // Check for shadows
  int in_shadow = 0;
  vec3_t shadow_orig = vec_add(hit_pos, vec_mul(normal, 0.005f));
  
  // Check if shadow ray intersects any other sphere
  for (int i = 0; i < NUM_SPHERES; i++) {
    float t_shadow;
    if (intersect_sphere(shadow_orig, light_dir, spheres[i], &t_shadow)) {
      // Calculate distance to light to ensure the occluding object is before the light
      float dist_to_light = ksqrt(vec_dot(vec_sub(light_pos, shadow_orig), vec_sub(light_pos, shadow_orig)));
      if (t_shadow < dist_to_light) {
        in_shadow = 1;
        break;
      }
    }
  }
  
  if (in_shadow) {
    diffuse = 0.0f; // Only ambient lighting in shadow
  }
  
  float ambient = 0.15f;
  float total_light = ambient + diffuse * 0.85f;
  
  // 6. Shade depending on hit object type
  if (hit_type == 4) {
    // Checkerboard floor pattern
    int ix = (int)(hit_pos.x + 100.0f);
    int iz = (int)(hit_pos.z + 100.0f);
    // Align negative coordinates properly
    if (hit_pos.x < 0.0f) ix--;
    if (hit_pos.z < 0.0f) iz--;
    
    int check = (ix + iz) & 1;
    
    // Grayscale ramp from index 16 (darkest) to 31 (brightest)
    int shade = (int)(total_light * 15.0f);
    if (shade < 0) shade = 0;
    if (shade > 15) shade = 15;
    
    if (check) {
      // Darker checker tiles
      int idx = 16 + shade / 2;
      if (idx > 23) idx = 23;
      return idx;
    } else {
      // Lighter checker tiles
      int idx = 20 + shade / 2;
      if (idx > 31) idx = 31;
      return idx;
    }
  }
  
  // Sphere shading
  int col_type = spheres[sphere_idx].color_type;
  if (col_type == 1) {
    // Red Sphere (Shadow = 4, Midtone = 12, Highlight = 15)
    if (total_light > 0.85f) return 15; // Specular Highlight
    if (total_light > 0.35f) return 12; // Diffuse
    return 4; // Shadow
  } else if (col_type == 2) {
    // Green Sphere (Shadow = 2, Midtone = 10, Highlight = 15)
    if (total_light > 0.85f) return 15;
    if (total_light > 0.35f) return 10;
    return 2;
  }
  
  return 0; // Fallback black
}

void _start() {
  // Setup standard spheres
  // 1. Red Sphere (Left)
  spheres[0].center = (vec3_t){-1.1f, 0.0f, 4.0f};
  spheres[0].radius = 0.8f;
  spheres[0].color_type = 1;
  
  // 2. Mirror Sphere (Right)
  spheres[1].center = (vec3_t){1.1f, 0.0f, 4.2f};
  spheres[1].radius = 0.8f;
  spheres[1].color_type = 3;
  
  // 3. Green Sphere (Center Front)
  spheres[2].center = (vec3_t){0.0f, -0.6f, 2.7f};
  spheres[2].radius = 0.4f;
  spheres[2].color_type = 2;

  // Resolution mode: 1 = Low (80x50), 2 = Med (160x100), 3 = High (320x200)
  int res_mode = 2; 
  int key_states[128] = {0};

  u_gfx_clear(0);
  
  int running = 1;
  while (running) {
    int w = 320;
    int h = 200;
    int step = 1;
    
    if (res_mode == 1) {
      w = 80; h = 50; step = 4;
    } else if (res_mode == 2) {
      w = 160; h = 100; step = 2;
    } else if (res_mode == 3) {
      w = 320; h = 200; step = 1;
    }

    // 1. Trace Rays & Draw direct to backbuffer
    float aspect_ratio = 320.0f / 200.0f;
    float fov_scale = 0.65f; // FOV zoom
    
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        // Map pixel coordinates to ray direction
        float px = ((float)x / (float)w) * 2.0f - 1.0f;
        float py = (1.0f - ((float)y / (float)h)) * 2.0f - 1.0f;
        
        px *= aspect_ratio * fov_scale;
        py *= fov_scale;
        
        vec3_t ray_dir = vec_normalize((vec3_t){px, py, 1.0f});
        
        // Raycast and shade
        unsigned char col = trace(cam_pos, ray_dir, 0);
        
        // Write block directly to backbuffer
        for (int dy = 0; dy < step; dy++) {
          for (int dx = 0; dx < step; dx++) {
            int sx = x * step + dx;
            int sy = y * step + dy;
            if (sx >= 0 && sx < 320 && sy >= 0 && sy < 200) {
              backbuffer[sy * 320 + sx] = col;
            }
          }
        }
      }
    }

    // 2. Render HUD Overlay
    char hud_res[20];
    if (res_mode == 1) u_strcpy(hud_res, "Res: 80x50");
    else if (res_mode == 2) u_strcpy(hud_res, "Res: 160x100");
    else if (res_mode == 3) u_strcpy(hud_res, "Res: 320x200");
    
    u_gfx_draw_string(76, 5, "NEXUS-3D REALTIME RAY TRACER", 14); // Centered yellow text
    u_gfx_draw_string(10, 188, "WASD:Fly  1-3:Res  Q/Esc:Exit", 10); // Green helper text on the left
    u_gfx_draw_string(240, 188, hud_res, 15); // White resolution info on the right
    
    // Draw crosshair at the mouse position (light cursor)
    int clicks, mx, my;
    u_mouse_get(&clicks, &mx, &my);
    
    // Draw a small cross to represent the light position
    u_gfx_draw_pixel(mx, my, 14); // Yellow dot
    u_draw_line(mx - 4, my, mx - 1, my, 14);
    u_draw_line(mx + 1, my, mx + 4, my, 14);
    u_draw_line(mx, my - 4, mx, my - 1, 14);
    u_draw_line(mx, my + 1, mx, my + 4, 14);
    
    // 3. Update Light Position from Mouse coordinates
    // Map mx (0..319) to light_x (-3.0..3.0)
    light_pos.x = ((float)mx / 320.0f) * 6.0f - 3.0f;
    // Map my (0..199) to light_y (-1.0..2.5)
    light_pos.y = (1.0f - (float)my / 200.0f) * 3.5f - 1.0f;

    u_gfx_swap();

    // 4. Read keyboard inputs
    while (u_key_has()) {
      unsigned char scancode = u_key_read();
      if (scancode == 0x10 || scancode == 0x01) { // 'Q' or Esc
        running = 0;
      }
      if (scancode < 128) {
        key_states[scancode] = 1;
      } else if (scancode >= 0x80 && (scancode & 0x7F) < 128) {
        key_states[scancode & 0x7F] = 0;
      }
    }

    // Process Camera Movement
    if (key_states[0x11]) { // 'W' - Forward
      cam_pos.z += 0.15f;
    }
    if (key_states[0x1F]) { // 'S' - Backward
      cam_pos.z -= 0.15f;
    }
    if (key_states[0x1E]) { // 'A' - Left
      cam_pos.x -= 0.15f;
    }
    if (key_states[0x20]) { // 'D' - Right
      cam_pos.x += 0.15f;
    }
    if (key_states[0x02]) { // '1' - Low resolution
      res_mode = 1;
    }
    if (key_states[0x03]) { // '2' - Medium resolution
      res_mode = 2;
    }
    if (key_states[0x04]) { // '3' - High resolution
      res_mode = 3;
    }
    if (key_states[0x10] || key_states[0x01]) { // 'Q' or Esc - Quit
      running = 0;
    }

    // Slight delay to keep loop running smoothly
    for (volatile int d = 0; d < 10000; d++);
  }

  // Clear keyboard buffer before exiting to avoid input leak in shell
  while (u_key_has()) {
    u_key_read();
  }

  u_exit();
}
