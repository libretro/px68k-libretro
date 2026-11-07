#ifndef _MIDI_SHADOW_H
#define _MIDI_SHADOW_H

#include <stddef.h>
#include <stdint.h>

#include "state.h"

/* Tracks what the MIDI destination (external synth or the internal SC-55)
 * has been told, so that the same "sound setup" can be re-created after a
 * state is loaded.
 *
 * The synth is not part of the emulated machine and cannot be rewound, so
 * after a load it still holds whatever mode, programs, volumes and SysEx
 * parameters it had before. This records the persistent part of what was
 * sent (the last reset SysEx and every parameter SysEx since, plus the last
 * bank/program/controller/pitch-bend/RPN/NRPN values of each channel) and can
 * play it back into a fresh synth. Notes are deliberately not recorded: they
 * are silenced on load.
 *
 * Messages use the same packing as midi_out_short_msg(): status | d1 << 8 |
 * d2 << 16. */

typedef void (*MIDIShadow_ShortFn)(size_t packed_msg);
typedef void (*MIDIShadow_LongFn)(const uint8_t *data, size_t len);

void MIDIShadow_Reset(void);
void MIDIShadow_Short(size_t packed_msg);
void MIDIShadow_Long(const uint8_t *data, size_t len);

/* Output source at save time, stored in the state: 0 = none, 1 = external
 * MIDI interface, 2 = internal SC-55. */
void MIDIShadow_SetSource(int source);

/* Source recorded in the state that was just loaded, or -1 if that state
 * carries no MIDI setup (states written by older builds). */
int MIDIShadow_GetLoadedSource(void);

/* Non-zero if the setup in the state just loaded differs from what has been
 * sent to the synth so far. */
int MIDIShadow_NeedsReplay(void);

/* Put the tracker back as it was before the last load (see midi_shadow.c). */
void MIDIShadow_RestoreLive(void);

void MIDIShadow_Replay(MIDIShadow_ShortFn short_fn, MIDIShadow_LongFn long_fn);

/* Normal (labelled) states only; fast Run-Ahead states don't carry it. */
int MIDIShadow_StateAction(StateMem *sm, int load, int data_only);

#endif
