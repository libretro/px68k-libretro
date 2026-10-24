#ifndef PX68K_ZIPDISK_H
#define PX68K_ZIPDISK_H
#include <stddef.h>
#include <stdint.h>

#define PX68K_ZIP_MAX_ENTRIES 10
#define PX68K_ZIP_NAME_MAX 4096
#define PX68K_ZIP_MENU_MAX_ENTRIES 4000
#define PX68K_ZIP_ARCHIVE_SLOTS 64

void px68k_zip_set_archive(const char *archive);
int px68k_zip_register_archive(const char *archive);
int px68k_zip_current_slot(void);
int px68k_zip_list(const char *archive, char names[PX68K_ZIP_MAX_ENTRIES][PX68K_ZIP_NAME_MAX]);
int px68k_zip_list_hdd(const char *archive, char names[PX68K_ZIP_MAX_ENTRIES][PX68K_ZIP_NAME_MAX]);
int px68k_zip_list_slot(int slot, char *names, size_t name_stride, int capacity);
int px68k_zip_make_virtual_path(int slot, const char *member, char *out, size_t out_size);
int px68k_zip_is_browse_path(const char *path);
int px68k_zip_browse_slot(const char *path);
int px68k_zip_get_archive_path(int slot, char *out, size_t out_size);
int px68k_zip_read_virtual(const char *virtual_path, uint8_t **data, size_t *size);
int px68k_zip_hdd_sector_read(const char *virtual_path, uint32_t sector, uint8_t *buffer);
int px68k_zip_hdd_sector_write(const char *virtual_path, uint32_t sector, const uint8_t *buffer);
void px68k_zip_hdd_clear(void);
void px68k_disk_menu_sync_zip(int slot, const char *member, int drive);
int px68k_is_zip_virtual_path(const char *path);
#endif
