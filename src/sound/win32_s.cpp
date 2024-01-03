/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_s.cpp Handling of sound for Windows. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../driver.h"
#include "../mixer.h"
#include "../core/alloc_func.hpp"
#include "../core/bitmath_func.hpp"
#include "../core/math_func.hpp"
#include "win32_s.h"
#include <windows.h>
#include <mmsystem.h>
#include <versionhelpers.h>
#include "../os/windows/win32.h"
#include "../thread.h"

#include "../safeguards.h"

static FSoundDriver_Win32 iFSoundDriver_Win32;

static HWAVEOUT _waveout;
static WAVEHDR _wave_hdr[2];
static int _bufsize;
static HANDLE _thread;
static DWORD _threadId;
static HANDLE _event;

static void PrepareHeader(WAVEHDR *hdr)
{
	hdr->dwBufferLength = _bufsize * 4;
	hdr->dwFlags = 0;
	hdr->lpData = MallocT<char>(_bufsize * 4);
	if (waveOutPrepareHeader(_waveout, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) throw "waveOutPrepareHeader failed";
}

static DWORD WINAPI SoundThread(LPVOID)
{
	SetCurrentThreadName("ottd:win-sound");

	do {
		for (WAVEHDR *hdr = _wave_hdr; hdr != endof(_wave_hdr); hdr++) {
			if ((hdr->dwFlags & WHDR_INQUEUE) != 0) continue;
			MxMixSamples(hdr->lpData, hdr->dwBufferLength / 4);
			if (waveOutWrite(_waveout, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
				MessageBox(nullptr, L"Sounds are disabled until restart.", L"waveOutWrite failed", MB_ICONINFORMATION);
				return 0;
			}
		}
		WaitForSingleObject(_event, INFINITE);
	} while (_waveout != nullptr);

	return 0;
}

const char *SoundDriver_Win32::Start(const StringList &parm)
{
	WAVEFORMATEX wfex;
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.wBitsPerSample = 16;
	wfex.nSamplesPerSec = GetDriverParamInt(parm, "hz", 44100);
	wfex.nBlockAlign = (wfex.nChannels * wfex.wBitsPerSample) / 8;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;

	/* Limit buffer size to prevent overflows. */
	_bufsize = GetDriverParamInt(parm, "bufsize", 8192);
	_bufsize = std::min<int>(_bufsize, UINT16_MAX);

	try {
		if (nullptr == (_event = CreateEvent(nullptr, FALSE, FALSE, nullptr))) throw "Failed to create event";

		if (waveOutOpen(&_waveout, WAVE_MAPPER, &wfex, (DWORD_PTR)_event, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) throw "waveOutOpen failed";

		MxInitialize(wfex.nSamplesPerSec);

		PrepareHeader(&_wave_hdr[0]);
		PrepareHeader(&_wave_hdr[1]);

		if (nullptr == (_thread = CreateThread(nullptr, 8192, SoundThread, 0, 0, &_threadId))) throw "Failed to create thread";
	} catch (const char *error) {
		this->Stop();
		return error;
	}

	return nullptr;
}

void SoundDriver_Win32::Stop()
{
	HWAVEOUT waveout = _waveout;

	/* Stop the sound thread. */
	_waveout = nullptr;
	SignalObjectAndWait(_event, _thread, INFINITE, FALSE);

	/* Close the sound device. */
	waveOutReset(waveout);
	waveOutUnprepareHeader(waveout, &_wave_hdr[0], sizeof(WAVEHDR));
	waveOutUnprepareHeader(waveout, &_wave_hdr[1], sizeof(WAVEHDR));
	waveOutClose(waveout);

	CloseHandle(_thread);
	CloseHandle(_event);
}
