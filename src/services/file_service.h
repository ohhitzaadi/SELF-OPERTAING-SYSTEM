#ifndef FILE_SERVICE_H
#define FILE_SERVICE_H

#include <stdint.h>

#define MAX_FILES 16
#define FILE_CONTENT_MAX 16384
#define FAT_DISK_BASE 0x20000

typedef struct {
  char name[32];
  char content[FILE_CONTENT_MAX];
  int size;
  int is_directory;
  int parent_idx; // -1 for root
  int used;
} file_t;

extern file_t files[MAX_FILES];
extern int current_dir_idx;

// FAT12 Structs
typedef struct __attribute__((packed)) {
  unsigned char jmp[3];
  char oem[8];
  unsigned short bytes_per_sector;
  unsigned char sectors_per_cluster;
  unsigned short reserved_sectors;
  unsigned char fat_count;
  unsigned short root_entry_count;
  unsigned short total_sectors_short;
  unsigned char media_descriptor;
  unsigned short sectors_per_fat;
  unsigned short sectors_per_track;
  unsigned short head_count;
  unsigned int hidden_sectors;
  unsigned int total_sectors_large;
} fat_bpb_t;

typedef struct __attribute__((packed)) {
  char name[11];
  unsigned char attr;
  unsigned char reserved;
  unsigned char creation_time_tenths;
  unsigned short creation_time;
  unsigned short creation_date;
  unsigned short last_access_date;
  unsigned short first_cluster_hi;
  unsigned short write_time;
  unsigned short write_date;
  unsigned short first_cluster_lo;
  unsigned int file_size;
} fat_dirent_t;

// API
void fs_init();
int fs_find(const char *name, int parent);
int fs_create(const char *name, int is_dir);
int fs_write_file(const char *name, const char *content);
int fs_delete_file(const char *name);

fat_dirent_t *fat_find_file(const char *filename);
int fat_read_file(const char *filename, unsigned char *buffer);
int fat16_get_file_size(const char *filename);
int fat16_read_file(const char *filename, unsigned char *buffer);
int fat16_write_file(const char *filename, const unsigned char *buffer, uint32_t size);
#endif
