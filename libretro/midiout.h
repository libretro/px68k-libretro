#ifndef	MIDIOUT_H
#define	MIDIOUT_H_

void midi_interface_init(void);
void update_variable_midi_interface(int running);

void midi_out_short_msg(size_t dwMsg);
void midi_out_long_msg(uint8_t *s, size_t len);
int midi_out_open(void **phmo);
void midi_out_flush(void);

#endif	/* MIDIOUT_H_ */
