#ifndef	MIDIOUT_H
#define	MIDIOUT_H_

void midi_interface_init(void);
void update_variable_midi_interface(int running);

void midi_out_short_msg(uint32_t msg);
void midi_out_long_msg(uint8_t *s, size_t len);
int midi_out_open(void);
void midi_out_close(void);
void midi_out_flush(void);

extern struct retro_midi_interface midi_cb;
extern retro_environment_t environ_cb;
extern retro_log_printf_t log_cb;

#endif	/* MIDIOUT_H_ */
