#ifndef PX68K_MT32EMU_LIBRETRO_H
#define PX68K_MT32EMU_LIBRETRO_H
#include <stddef.h>
#include <stdint.h>
#include <libretro.h>
#ifdef __cplusplus
extern "C" {
#endif
void px68k_mt32_set_log_callback(retro_log_printf_t callback);
int px68k_mt32_configure(int emulation, const char *system_dir, const char *content_dir);
int px68k_mt32_enabled(void);
void px68k_mt32_midi_short(size_t msg);
void px68k_mt32_midi_long(const uint8_t *data, size_t len);
void px68k_mt32_mix(int16_t *stereo, size_t frames, int volume_percent);
void px68k_mt32_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
