#ifndef PX68K_SC55_LIBRETRO_H
#define PX68K_SC55_LIBRETRO_H
#include <stddef.h>
#include <stdint.h>
void px68k_sc55_configure(int enabled, const char *system_dir, const char *content_dir);
int px68k_sc55_enabled(void);
void px68k_sc55_midi_short(size_t packed_msg);
void px68k_sc55_midi_long(const uint8_t *data, size_t len);
void px68k_sc55_mix(int16_t *stereo, size_t frames);
void px68k_sc55_set_volume(int volume_percent);
void px68k_sc55_shutdown(void);
#endif
