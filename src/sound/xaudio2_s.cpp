/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file xaudio2_s.cpp XAudio2 sound driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../driver.h"
#include "../mixer.h"
#include "../debug.h"
#include "../core/bitmath_func.hpp"
#include "../core/math_func.hpp"

/* Windows 8 SDK required for XAudio2 */
#undef NTDDI_VERSION
#undef _WIN32_WINNT

#define NTDDI_VERSION    NTDDI_WIN8
#define _WIN32_WINNT     _WIN32_WINNT_WIN8

#include "xaudio2_s.h"

#include <windows.h>
#include <mmsystem.h>
#include <wrl\client.h>
#include <xaudio2.h>

using Microsoft::WRL::ComPtr;

#include "../os/windows/win32.h"
#include "../safeguards.h"

/* Definition of the "XAudio2Create" call used to initialise XAudio2 */
typedef HRESULT(__stdcall *API_XAudio2Create)(_Outptr_ IXAudio2 **ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor);

static FSoundDriver_XAudio2 iFSoundDriver_XAudio2;

/**
 * Implementation of the IXAudio2VoiceCallback interface.
 * Provides buffered audio to XAudio2 from the OpenTTD mixer.
 */
class StreamingVoiceContext : public IXAudio2VoiceCallback
{
private:
	std::vector<BYTE> buffer;

public:
	IXAudio2SourceVoice *source_voice = nullptr;

	StreamingVoiceContext(int buffer_length)
	{
		this->buffer.resize(buffer_length);
	}

	virtual ~StreamingVoiceContext() = default;

	HRESULT SubmitBuffer()
	{
		/* Ensure we do have a valid voice */
		if (this->source_voice == nullptr) {
			return E_FAIL;
		}

		MxMixSamples(this->buffer.data(), static_cast<uint>(this->buffer.size() / 4));

		XAUDIO2_BUFFER buf = { 0 };
		buf.AudioBytes = static_cast<UINT32>(this->buffer.size());
		buf.pAudioData = this->buffer.data();

		return source_voice->SubmitSourceBuffer(&buf);
	}

	STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) override
	{
	}

	STDMETHOD_(void, OnVoiceProcessingPassEnd)() override
	{
	}

	STDMETHOD_(void, OnStreamEnd)() override
	{
	}

	STDMETHOD_(void, OnBufferStart)(void*) override
	{
	}

	STDMETHOD_(void, OnBufferEnd)(void*) override
	{
		SubmitBuffer();
	}

	STDMETHOD_(void, OnLoopEnd)(void*) override
	{
	}

	STDMETHOD_(void, OnVoiceError)(void*, HRESULT) override
	{
	}
};

static HMODULE _xaudio_dll_handle;
static IXAudio2SourceVoice *_source_voice = nullptr;
static IXAudio2MasteringVoice *_mastering_voice = nullptr;
static ComPtr<IXAudio2> _xaudio2;
static std::unique_ptr<StreamingVoiceContext> _voice_context;

/** Create XAudio2 context with SEH exception checking. */
static HRESULT CreateXAudio(API_XAudio2Create xAudio2Create)
{
	HRESULT hr;
	__try {
		UINT32 flags = 0;
		hr = xAudio2Create(_xaudio2.GetAddressOf(), flags, XAUDIO2_DEFAULT_PROCESSOR);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		hr = GetExceptionCode();
	}

	return hr;
}

/**
 * Initialises the XAudio2 driver.
 *
 * @param parm Driver parameters.
 * @return An error message if unsuccessful, or std::nullopt otherwise.
 *
 */
std::optional<std::string_view> SoundDriver_XAudio2::Start(const StringList &parm)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	if (FAILED(hr)) {
		Debug(driver, 0, "xaudio2_s: CoInitializeEx failed ({:08x})", (uint)hr);
		return "Failed to initialise COM";
	}

	_xaudio_dll_handle = LoadLibraryA(XAUDIO2_DLL_A);

	if (_xaudio_dll_handle == nullptr) {
		CoUninitialize();

		Debug(driver, 0, "xaudio2_s: Unable to load " XAUDIO2_DLL_A);
		return "Failed to load XAudio2 DLL";
	}

	API_XAudio2Create xAudio2Create = (API_XAudio2Create) GetProcAddress(_xaudio_dll_handle, "XAudio2Create");

	if (xAudio2Create == nullptr) {
		FreeLibrary(_xaudio_dll_handle);
		CoUninitialize();

		Debug(driver, 0, "xaudio2_s: Unable to find XAudio2Create function in DLL");
		return "Failed to load XAudio2 DLL";
	}

	/* Create the XAudio engine */
	hr = CreateXAudio(xAudio2Create);

	if (FAILED(hr)) {
		FreeLibrary(_xaudio_dll_handle);
		CoUninitialize();

		Debug(driver, 0, "xaudio2_s: XAudio2Create failed ({:08x})", (uint)hr);
		return "Failed to initialise the XAudio2 engine";
	}

	/* Create a mastering voice */
	hr = _xaudio2->CreateMasteringVoice(&_mastering_voice);

	if (FAILED(hr)) {
		_xaudio2.Reset();
		FreeLibrary(_xaudio_dll_handle);
		CoUninitialize();

		Debug(driver, 0, "xaudio2_s: CreateMasteringVoice failed ({:08x})", (uint)hr);
		return "Failed to create a mastering voice";
	}

	/* Create a source voice to stream our audio */
	WAVEFORMATEX wfex;

	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.wBitsPerSample = 16;
	wfex.nSamplesPerSec = GetDriverParamInt(parm, "hz", 44100);
	wfex.nBlockAlign = (wfex.nChannels * wfex.wBitsPerSample) / 8;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;

	/* Limit buffer size to prevent overflows */
	int bufsize = GetDriverParamInt(parm, "samples", 1024);
	bufsize = std::min<int>(bufsize, UINT16_MAX);

	_voice_context = std::make_unique<StreamingVoiceContext>(bufsize * 4);

	if (_voice_context == nullptr) {
		_mastering_voice->DestroyVoice();
		_xaudio2.Reset();
		FreeLibrary(_xaudio_dll_handle);
		CoUninitialize();

		return "Failed to create streaming voice context";
	}

	hr = _xaudio2->CreateSourceVoice(&_source_voice, &wfex, 0, 1.0f, _voice_context.get());

	if (FAILED(hr)) {
		_mastering_voice->DestroyVoice();
		_xaudio2.Reset();
		FreeLibrary(_xaudio_dll_handle);
		CoUninitialize();

		Debug(driver, 0, "xaudio2_s: CreateSourceVoice failed ({:08x})", (uint)hr);
		return "Failed to create a source voice";
	}

	_voice_context->source_voice = _source_voice;
	hr = _source_voice->Start(0, 0);

	if (FAILED(hr)) {
		Debug(driver, 0, "xaudio2_s: _source_voice->Start failed ({:08x})", (uint)hr);

		Stop();
		return "Failed to start the source voice";
	}

	MxInitialize(wfex.nSamplesPerSec);

	/* Submit the first buffer */
	hr = _voice_context->SubmitBuffer();

	if (FAILED(hr)) {
		Debug(driver, 0, "xaudio2_s: _voice_context->SubmitBuffer failed ({:08x})", (uint)hr);

		Stop();
		return "Failed to submit the first audio buffer";
	}

	return std::nullopt;
}

/**
 * Terminates the XAudio2 driver.
 */
void SoundDriver_XAudio2::Stop()
{
	/* Clean up XAudio2 */
	_source_voice->DestroyVoice();

	_voice_context = nullptr;

	_mastering_voice->DestroyVoice();

	_xaudio2.Reset();

	FreeLibrary(_xaudio_dll_handle);
	CoUninitialize();
}
