#include <stdint.h>
#include <stdio.h>
#include <libretro.h>

#include "midiout.h"
#include "midiout_winmm.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>

static HMIDIOUT hOut;
static HANDLE hCallbackEvent;
static MIDIHDR hHdr = {0};

static void midi_error(const char *prefix, MMRESULT result)
{
    LPSTR error[MAXERRORLENGTH];

    if (midiOutGetErrorText(result, (LPSTR)error, MAXERRORLENGTH) == MMSYSERR_NOERROR)
    {
        log_cb(RETRO_LOG_ERROR, "%s: %s\n", prefix, error);
    }
    else
    {
        log_cb(RETRO_LOG_ERROR, "%s: Unknown error\n", prefix);
    }
}

static void CALLBACK winmm_proc(HMIDIOUT hmo, UINT wMsg, DWORD_PTR dwInstance,
                                 DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (wMsg == MOM_DONE)
    {
        SetEvent(hCallbackEvent);
    }
}

void winmm_send_short_msg(uint32_t msg)
{
    MMRESULT result;

    while (midiOutUnprepareHeader(hOut, &hHdr, sizeof(hHdr)) == MIDIERR_STILLPLAYING)
    {
        ;
    }

    result = midiOutShortMsg(hOut, msg);
    if (result != MMSYSERR_NOERROR)
    {
        midi_error("winmm_send_short_msg", result);
    }
}

void winmm_send_long_msg(const uint8_t *msg, unsigned int length)
{
    MMRESULT result;

    if (WaitForSingleObject(hCallbackEvent, 2000) == WAIT_OBJECT_0)
    {
        result = midiOutUnprepareHeader(hOut, &hHdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            midi_error("winmm_send_long_msg", result);
        }
    }

    hHdr.lpData = (LPSTR)msg;
    hHdr.dwBufferLength = length;
    hHdr.dwBytesRecorded = length;
    hHdr.dwFlags = 0;
    hHdr.dwUser = 0;

    result = midiOutPrepareHeader(hOut, &hHdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR)
    {
        midi_error("winmm_send_long_msg", result);
        return;
    }

    result = midiOutLongMsg(hOut, &hHdr, sizeof(MIDIHDR));
    if (result != MMSYSERR_NOERROR)
    {
        midi_error("winmm_send_long_msg", result);
        return;
    }
}

int winmm_device_count(void)
{
    return midiOutGetNumDevs();
}

const char *winmm_device_name(int device)
{
    MMRESULT result;
    MIDIOUTCAPS caps;

    result = midiOutGetDevCaps(device, &caps, sizeof(caps));
    if (result == MMSYSERR_NOERROR)
    {
        char *str = malloc(strlen(caps.szPname) + 1);
        sprintf(str, "%s", caps.szPname);
        return str;
    }
    else
    {
        midi_error("winmm_device_name", result);
    }

    return NULL;
}

int winmm_device_open(int device)
{
    MMRESULT result = midiOutOpen(&hOut, device, (DWORD_PTR)&winmm_proc,
                                  (DWORD_PTR)NULL, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        midi_error("winmm_device_open", result);
        return 0;
    }

    hCallbackEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    return 1;
}

void winmm_device_close(void)
{
    MMRESULT result;
    while (midiOutUnprepareHeader(hOut, &hHdr, sizeof(hHdr)) == MIDIERR_STILLPLAYING)
    {
        ;
    }
    result = midiOutReset(hOut);
    if (result != MMSYSERR_NOERROR)
    {
        midi_error("winmm_device_close", result);
    }
    result = midiOutClose(hOut);
    if (result != MMSYSERR_NOERROR)
    {
        midi_error("winmm_device_close", result);
    }
    CloseHandle(hCallbackEvent);
}

#endif /* _WIN32 */