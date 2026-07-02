#include "file_service.h"
#include "kernel/mem.h" // IWYU pragma: keep
#include "kernel/drivers/ata.h"

// Diagnostic logs
extern void kprint(const char *str);
extern void kprint_color(const char *str, unsigned char color);

file_t files[MAX_FILES];
int current_dir_idx = -1; // -1 = root "/"

// FAT12 helper
unsigned short fat12_get_next_cluster(int c) {
  fat_bpb_t *bpb = (fat_bpb_t *)FAT_DISK_BASE;
  unsigned char *fat_table =
      (unsigned char *)(FAT_DISK_BASE + bpb->reserved_sectors * 512);
  int offset = (c * 3) / 2;
  unsigned short val = fat_table[offset] | (fat_table[offset + 1] << 8);
  if (c & 1) {
    return val >> 4;
  } else {
    return val & 0x0FFF;
  }
}

fat_dirent_t *fat_find_file(const char *filename) {
  fat_bpb_t *bpb = (fat_bpb_t *)FAT_DISK_BASE;
  int root_sec =
      bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
  int root_entries = bpb->root_entry_count;

  char fat_name[11];
  memset(fat_name, ' ', 11);
  int len = strlen(filename);
  int dot_idx = -1;
  for (int i = 0; i < len; i++) {
    if (filename[i] == '.') {
      dot_idx = i;
      break;
    }
  }
  if (dot_idx == -1) {
    for (int i = 0; i < len && i < 8; i++) {
      fat_name[i] = filename[i];
    }
  } else {
    for (int i = 0; i < dot_idx && i < 8; i++) {
      fat_name[i] = filename[i];
    }
    for (int i = 0; i < 3 && (dot_idx + 1 + i) < len; i++) {
      fat_name[8 + i] = filename[dot_idx + 1 + i];
    }
  }
  for (int i = 0; i < 11; i++) {
    if (fat_name[i] >= 'a' && fat_name[i] <= 'z') {
      fat_name[i] = fat_name[i] - 'a' + 'A';
    }
  }

  fat_dirent_t *dirents = (fat_dirent_t *)(FAT_DISK_BASE + root_sec * 512);
  for (int i = 0; i < root_entries; i++) {
    if (dirents[i].name[0] == 0x00)
      break;
    if ((unsigned char)dirents[i].name[0] == 0xE5)
      continue;
    if (dirents[i].attr & 0x08)
      continue;

    int match = 1;
    for (int j = 0; j < 11; j++) {
      if (dirents[i].name[j] != fat_name[j]) {
        match = 0;
        break;
      }
    }
    if (match) {
      return &dirents[i];
    }
  }
  return 0;
}

int fat16_find_file(const char *filename, uint16_t *start_cluster, uint32_t *file_size) {
  char fat_name[11];
  memset(fat_name, ' ', 11);
  int len = strlen(filename);
  int dot_idx = -1;
  for (int i = 0; i < len; i++) {
    if (filename[i] == '.') {
      dot_idx = i;
      break;
    }
  }
  if (dot_idx == -1) {
    for (int i = 0; i < len && i < 8; i++) {
      fat_name[i] = filename[i];
    }
  } else {
    for (int i = 0; i < dot_idx && i < 8; i++) {
      fat_name[i] = filename[i];
    }
    for (int i = 0; i < 3 && (dot_idx + 1 + i) < len; i++) {
      fat_name[8 + i] = filename[dot_idx + 1 + i];
    }
  }
  for (int i = 0; i < 11; i++) {
    if (fat_name[i] >= 'a' && fat_name[i] <= 'z') {
      fat_name[i] = fat_name[i] - 'a' + 'A';
    }
  }

  uint8_t sector_buf[512];
  // 32 sectors of root directory (FAT16 default)
  for (int s = 0; s < 32; s++) {
    ata_read_sector(518 + s, sector_buf);
    
    // Each sector has 16 entries (512 / 32)
    for (int e = 0; e < 16; e++) {
      int offset = e * 32;
      if (sector_buf[offset] == 0x00) {
        return -1; // End of directory
      }
      if (sector_buf[offset] == 0xE5) {
        continue; // Deleted entry
      }
      if (sector_buf[offset + 11] & 0x08) {
        continue; // Volume label
      }
      
      // Compare names
      int match = 1;
      for (int j = 0; j < 11; j++) {
        if (sector_buf[offset + j] != fat_name[j]) {
          match = 0;
          break;
        }
      }
      
      if (match) {
        *start_cluster = *(uint16_t *)(&sector_buf[offset + 26]);
        *file_size = *(uint32_t *)(&sector_buf[offset + 28]);
        return 0; // Found!
      }
    }
  }
  return -1;
}

int fat16_get_file_size(const char *filename) {
  uint16_t start_cluster;
  uint32_t file_size;
  if (fat16_find_file(filename, &start_cluster, &file_size) == 0) {
    return (int)file_size;
  }
  return -1;
}

int fat16_read_file(const char *filename, unsigned char *buffer) {
  uint16_t cluster;
  uint32_t file_size;
  if (fat16_find_file(filename, &cluster, &file_size) != 0) {
    return -1;
  }
  
  uint8_t fat_buf[512];
  uint32_t bytes_read = 0;
  
  // FAT16 parameters: Start Sector = 4, Data Start = 550, Sectors per Cluster = 8 (4096 bytes)
  while (bytes_read < file_size && cluster < 0xFFF8) {
    uint32_t start_sector = 550 + (cluster - 2) * 8;
    
    // Read cluster sectors (8 sectors = 4096 bytes)
    for (int s = 0; s < 8; s++) {
      if (bytes_read >= file_size) break;
      
      uint32_t to_copy = file_size - bytes_read;
      if (to_copy > 512) to_copy = 512;
      
      uint8_t sector_temp[512];
      ata_read_sector(start_sector + s, sector_temp);
      memcpy(buffer + bytes_read, sector_temp, to_copy);
      bytes_read += to_copy;
    }
    
    // Get next cluster from FAT
    uint32_t fat_sector = 4 + (cluster * 2) / 512;
    uint32_t fat_offset = (cluster * 2) % 512;
    
    ata_read_sector(fat_sector, fat_buf);
    cluster = *(uint16_t *)(&fat_buf[fat_offset]);
  }
  
  return (int)bytes_read;
}

int fat_read_file(const char *filename, unsigned char *buffer) {
  fat_dirent_t *entry = fat_find_file(filename);
  if (!entry) {
    // Fallback to secondary hard drive (FAT16)
    return fat16_read_file(filename, buffer);
  }

  fat_bpb_t *bpb = (fat_bpb_t *)FAT_DISK_BASE;
  int root_sec =
      bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
  int root_sec_count = ((bpb->root_entry_count * 32) + 511) / 512;
  int data_sec_start = root_sec + root_sec_count;

  unsigned short cluster = entry->first_cluster_lo;
  int bytes_read = 0;
  int file_size = entry->file_size;
  int cluster_size = bpb->sectors_per_cluster * 512;

  while (bytes_read < file_size && cluster < 0x0FF8) {
    int sector = data_sec_start + (cluster - 2) * bpb->sectors_per_cluster;
    unsigned char *cluster_data =
        (unsigned char *)(FAT_DISK_BASE + sector * 512);
    int to_copy = file_size - bytes_read;
    if (to_copy > cluster_size) {
      to_copy = cluster_size;
    }
    memcpy(buffer + bytes_read, cluster_data, to_copy);
    bytes_read += to_copy;
    cluster = fat12_get_next_cluster(cluster);
  }
  return bytes_read;
}

void fs_init() {
  memset(files, 0, sizeof(files));

  // "/home" directory
  strcpy(files[0].name, "home");
  files[0].is_directory = 1;
  files[0].parent_idx = -1;
  files[0].used = 1;

  // "/readme.txt" file
  strcpy(files[1].name, "readme.txt");
  strcpy(files[1].content, "Welcome to NexusOS! A real x86 C/Assembly Kernel "
                           "with multitasking, filesystem, and paging.");
  files[1].size = strlen(files[1].content);
  files[1].is_directory = 0;
  files[1].parent_idx = -1;
  files[1].used = 1;

  // "/home/user" directory
  strcpy(files[2].name, "user");
  files[2].is_directory = 1;
  files[2].parent_idx = 0; // Parent index = 0 (/home)
  files[2].used = 1;

  // "/home/user/about.txt" file
  strcpy(files[3].name, "about.txt");
  strcpy(files[3].content, "Created with custom boot sector, page allocator, "
                           "context-switcher, and keyboard poller.");
  files[3].size = strlen(files[3].content);
  files[3].is_directory = 0;
  files[3].parent_idx = 2; // Parent index = 2 (user)
  files[3].used = 1;

  // "/doom.wad" file (136 bytes mock Doom WAD)
  strcpy(files[4].name, "doom.wad");
  files[4].is_directory = 0;
  files[4].parent_idx = -1; // Root directory
  files[4].used = 1;

  unsigned char *wad = (unsigned char *)files[4].content;
  wad[0] = 'I'; wad[1] = 'W'; wad[2] = 'A'; wad[3] = 'D';
  wad[4] = 3; wad[5] = 0; wad[6] = 0; wad[7] = 0;
  wad[8] = 88; wad[9] = 0; wad[10] = 0; wad[11] = 0;

  for (int i = 0; i < 64; i++) {
    wad[12 + i] = (unsigned char)(i * 2);
  }
  for (int i = 0; i < 12; i++) {
    wad[76 + i] = (unsigned char)(i + 32);
  }

  wad[88] = 12; wad[89] = 0; wad[90] = 0; wad[91] = 0;
  wad[92] = 64; wad[93] = 0; wad[94] = 0; wad[95] = 0;
  strcpy((char *)&wad[96], "PLAYPAL");

  wad[104] = 76; wad[105] = 0; wad[106] = 0; wad[107] = 0;
  wad[108] = 12; wad[109] = 0; wad[110] = 0; wad[111] = 0;
  strcpy((char *)&wad[112], "COLORMAP");

  wad[120] = 88; wad[121] = 0; wad[122] = 0; wad[123] = 0;
  wad[124] = 0; wad[125] = 0; wad[126] = 0; wad[127] = 0;
  strcpy((char *)&wad[128], "E1M1");

  files[4].size = 136;
}

int fs_find(const char *name, int parent) {
  for (int i = 0; i < MAX_FILES; i++) {
    if (files[i].used && files[i].parent_idx == parent) {
      if (strcmp(files[i].name, name) == 0) {
        return i;
      }
    }
  }
  return -1;
}

int fs_create(const char *name, int is_dir) {
  if (fs_find(name, current_dir_idx) != -1) {
    kprint("[FS] Error: File or directory already exists.\n");
    return -1;
  }

  for (int i = 0; i < MAX_FILES; i++) {
    if (!files[i].used) {
      strcpy(files[i].name, name);
      files[i].is_directory = is_dir;
      files[i].parent_idx = current_dir_idx;
      files[i].content[0] = '\0';
      files[i].size = 0;
      files[i].used = 1;
      return i;
    }
  }
  kprint("[FS] Error: RAM file system is full.\n");
  return -1;
}

int fat16_write_file(const char *filename, const unsigned char *buffer, uint32_t size) {
  uint16_t cluster;
  uint32_t file_size;
  if (fat16_find_file(filename, &cluster, &file_size) != 0) {
    return -1;
  }

  if (size > file_size) size = file_size;

  uint8_t fat_buf[512];
  uint32_t bytes_written = 0;

  // FAT16 parameters: Start Sector = 4, Data Start = 550, Sectors per Cluster = 8 (4096 bytes)
  while (bytes_written < size && cluster < 0xFFF8) {
    uint32_t start_sector = 550 + (cluster - 2) * 8;

    // Write to cluster sectors (8 sectors = 4096 bytes)
    for (int s = 0; s < 8; s++) {
      if (bytes_written >= size) break;

      uint32_t to_write = size - bytes_written;
      if (to_write > 512) to_write = 512;

      uint8_t sector_temp[512];
      if (to_write < 512) {
        ata_read_sector(start_sector + s, sector_temp);
      }
      memcpy(sector_temp, buffer + bytes_written, to_write);
      ata_write_sector(start_sector + s, sector_temp);
      bytes_written += to_write;
    }

    // Get next cluster from FAT
    uint32_t fat_sector = 4 + (cluster * 2) / 512;
    uint32_t fat_offset = (cluster * 2) % 512;

    ata_read_sector(fat_sector, fat_buf);
    cluster = *(uint16_t *)(&fat_buf[fat_offset]);
  }

  return (int)bytes_written;
}

int fs_write_file(const char *name, const char *content) {
  // 1. Try writing to hard disk (FAT16) first if the file is pre-allocated on hard disk
  uint16_t cluster;
  uint32_t file_size;
  if (fat16_find_file(name, &cluster, &file_size) == 0) {
    int write_size = strlen(content);
    if (strcmp(name, "PAINT.RAW") == 0) {
      write_size = 12800; // Paint canvas is always 12800 bytes binary data
    }
    return fat16_write_file(name, (const unsigned char *)content, write_size);
  }

  // 2. Otherwise fall back to RAM disk
  int idx = fs_find(name, current_dir_idx);
  if (idx == -1) {
    idx = fs_create(name, 0);
  }
  if (idx != -1) {
    if (files[idx].is_directory) {
      kprint("[FS] Error: Cannot write to a directory.\n");
      return -1;
    }
    int len = strlen(content);
    if (len >= FILE_CONTENT_MAX)
      len = FILE_CONTENT_MAX - 1;
    int i;
    for (i = 0; i < len; i++) {
      files[idx].content[i] = content[i];
    }
    files[idx].content[i] = '\0';
    files[idx].size = len;
    return 0;
  }
  return -1;
}

int fs_delete_file(const char *name) {
  int idx = fs_find(name, current_dir_idx);
  if (idx != -1) {
    files[idx].used = 0;
    return 0;
  }
  kprint("[FS] Error: File not found.\n");
  return -1;
}
