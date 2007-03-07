/* $Id$ */

#include "../stdafx.h"
#include "../openttd.h"
#include "../driver.h"
#include "../functions.h"
#include "../mixer.h"
#include "../helpers.hpp"
#include "win32_s.h"
#include <windows.h>
#include <mmsystem.h>

static HWAVEOUT _waveout;
static WAVEHDR _wave_hdr[2];
static int _bufsize;

static void PrepareHeader(WAVEHDR *hdr)
{
	hdr->dwBufferLength = _bufsize * 4;
	hdr->dwFlags = 0;
	hdr->lpData = MallocT<char>(_bufsize * 4);
	if (hdr->lpData == NULL ||
			waveOutPrepareHeader(_waveout, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		error("waveOutPrepareHeader failed");
}

static void FillHeaders()
{
	WAVEHDR *hdr;

	for (hdr = _wave_hdr; hdr != endof(_wave_hdr); hdr++) {
		if (!(hdr->dwFlags & WHDR_INQUEUE)) {
			MxMixSamples(hdr->lpData, hdr->dwBufferLength / 4);
			if (waveOutWrite(_waveout, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
				error("waveOutWrite failed");
		}
	}
}

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
	DWORD dwParam1, DWORD dwParam2)
{
	switch (uMsg) {
		case WOM_DONE:
			if (_waveout != NULL) FillHeaders();
			break;
		default: break;
	}
}

static const char *Win32SoundStart(const char* const* parm)
{
	WAVEFORMATEX wfex;
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.wBitsPerSample = 16;
	wfex.nSamplesPerSec = GetDriverParamInt(parm, "hz", 11025);
	wfex.nBlockAlign = (wfex.nChannels * wfex.wBitsPerSample) / 8;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;

	_bufsize = GetDriverParamInt(parm, "bufsize", 1024);

	if (waveOutOpen(&_waveout, WAVE_MAPPER, &wfex, (DWORD_PTR)&waveOutProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
		return "waveOutOpen failed";

	PrepareHeader(&_wave_hdr[0]);
	PrepareHeader(&_wave_hdr[1]);
	FillHeaders();
	return NULL;
}

static void Win32SoundStop()
{
	HWAVEOUT waveout = _waveout;

	_waveout = NULL;
	waveOutReset(waveout);
	waveOutUnprepareHeader(waveout, &_wave_hdr[0], sizeof(WAVEHDR));
	waveOutUnprepareHeader(waveout, &_wave_hdr[1], sizeof(WAVEHDR));
	waveOutClose(waveout);
}

const HalSoundDriver _win32_sound_driver = {
	Win32SoundStart,
	Win32SoundStop,
};
