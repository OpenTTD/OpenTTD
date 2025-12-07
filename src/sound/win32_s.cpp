/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file win32_s.cpp Handling of sound for Windows. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../driver.h"
#include "../mixer.h"
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

using HeaderDataPair = std::pair<WAVEHDR, std::unique_ptr<CHAR[]>>;

static HWAVEOUT _waveout;
static HeaderDataPair _wave_hdr[2];
static int _bufsize;
static HANDLE _thread;
static DWORD _threadId;
static HANDLE _event;

static void PrepareHeader(HeaderDataPair &hdr)
{
	hdr.second = std::make_unique<CHAR[]>(_bufsize * 4);
	hdr.first.dwBufferLength = _bufsize * 4;
	hdr.first.dwFlags = 0;
	hdr.first.lpData = hdr.second.get();
	if (waveOutPrepareHeader(_waveout, &hdr.first, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) throw "waveOutPrepareHeader failed"sv;
}

static DWORD WINAPI SoundThread(LPVOID)
{
	SetCurrentThreadName("ottd:win-sound");

	do {
		for (auto &hdr : _wave_hdr) {
			if ((hdr.first.dwFlags & WHDR_INQUEUE) != 0) continue;
			MxMixSamples(hdr.first.lpData, hdr.first.dwBufferLength / 4);
			if (waveOutWrite(_waveout, &hdr.first, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
				MessageBox(nullptr, L"Sounds are disabled until restart.", L"waveOutWrite failed", MB_ICONINFORMATION);
				return 0;
			}
		}
		WaitForSingleObject(_event, INFINITE);
	} while (_waveout != nullptr);

	return 0;
}

std::optional<std::string_view> SoundDriver_Win32::Start(const StringList &parm)
{
	WAVEFORMATEX wfex;
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.wBitsPerSample = 16;
	wfex.nSamplesPerSec = GetDriverParamInt(parm, "hz", 44100);
	wfex.nBlockAlign = (wfex.nChannels * wfex.wBitsPerSample) / 8;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;

	/* Limit buffer size to prevent overflows. */
	_bufsize = GetDriverParamInt(parm, "samples", 1024);
	_bufsize = std::min<int>(_bufsize, UINT16_MAX);

	try {
		if (nullptr == (_event = CreateEvent(nullptr, FALSE, FALSE, nullptr))) throw "Failed to create event"sv;

		if (waveOutOpen(&_waveout, WAVE_MAPPER, &wfex, (DWORD_PTR)_event, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) throw "waveOutOpen failed"sv;

		MxInitialize(wfex.nSamplesPerSec);

		for (auto &hdr : _wave_hdr) PrepareHeader(hdr);

		if (nullptr == (_thread = CreateThread(nullptr, 8192, SoundThread, 0, 0, &_threadId))) throw "Failed to create thread"sv;
	} catch (std::string_view error) {
		this->Stop();
		return error;
	}

	return std::nullopt;
}

void SoundDriver_Win32::Stop()
{
	HWAVEOUT waveout = _waveout;

	/* Stop the sound thread. */
	_waveout = nullptr;
	SignalObjectAndWait(_event, _thread, INFINITE, FALSE);

	/* Close the sound device. */
	waveOutReset(waveout);
	for (auto &hdr : _wave_hdr) {
		waveOutUnprepareHeader(waveout, &hdr.first, sizeof(WAVEHDR));
		hdr.second.reset();
	}
	waveOutClose(waveout);

	CloseHandle(_thread);
	CloseHandle(_event);
}
