#ifndef PX68K_MIDIEMU_LIBRETRO_H
#define PX68K_MIDIEMU_LIBRETRO_H
#include <stddef.h>
#include <stdint.h>
#include <libretro.h>
enum { PX68K_MIDIEMU_DISABLED = 0, PX68K_MIDIEMU_SC55, PX68K_MIDIEMU_MT32, PX68K_MIDIEMU_CM32L };
void px68k_midiemu_set_log_callback(retro_log_printf_t callback);
void px68k_midiemu_configure(int emulation, const char *system_dir, const char *content_dir);
int px68k_midiemu_enabled(void);
void px68k_midiemu_midi_short(size_t packed_msg);
void px68k_midiemu_midi_long(const uint8_t *data, size_t len);
void px68k_midiemu_mix(int16_t *stereo, size_t frames);
void px68k_midiemu_set_volume(int volume_percent);
void px68k_midiemu_shutdown(void);
#endif
