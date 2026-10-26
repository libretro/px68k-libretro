#ifndef PX68K_ZIPDISK_H
#define PX68K_ZIPDISK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Longest member name accepted: "zip://@NN/" plus the name has to fit
 * the MAX_PATH (256) drive image paths. */
#define PX68K_ZIP_NAME_MAX 240

int px68k_zip_register_archive(const char *archive);
void px68k_zip_pin(int slot);
void px68k_zip_reset(void);

/* Sorted floppy (hdd == 0) or HDF (hdd != 0) members of an archive. A
 * name stays valid until another slot is queried. */
int px68k_zip_count(int slot, int hdd);
const char *px68k_zip_name(int slot, int hdd, int index);

int px68k_is_zip_virtual_path(const char *path);
int px68k_zip_virtual_slot(const char *path);
int px68k_zip_make_virtual_path(int slot, const char *member, char *out, size_t out_size);
int px68k_zip_is_browse_path(const char *path);
int px68k_zip_browse_slot(const char *path);
int px68k_zip_get_archive_path(int slot, char *out, size_t out_size);

/* Inflates a member into the session cache so later opens are free. */
int px68k_zip_preload(const char *virtual_path);

/* Memory-backed handles behind dosio's file_* calls for zip:// paths.
 * HDF members stay resident until px68k_zip_reset(), so SASI writes last
 * for the session; floppy members are freed on their last close. */
void *px68k_zip_open(const char *virtual_path);
int px68k_zip_owns(const void *handle);
size_t px68k_zip_seek(void *handle, long pos, int mode);
size_t px68k_zip_read(void *handle, void *data, size_t len);
size_t px68k_zip_write(void *handle, const void *data, size_t len);
int64_t px68k_zip_tell(void *handle);
void px68k_zip_close(void *handle);

#ifdef __cplusplus
}
#endif

#endif
