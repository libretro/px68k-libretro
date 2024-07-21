#ifndef MIDIOUT_WINMM_H_
#define MIDIOUT_WINMM_H_

void winmm_send_short_msg(uint32_t msg);
void winmm_send_long_msg(const uint8_t *msg, unsigned int length);
int winmm_device_count(void);
const char *winmm_device_name(int device);
int winmm_device_open(int device);
void winmm_device_close(void);

#endif /* MIDIOUT_WINMM_H_ */