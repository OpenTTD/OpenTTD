/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dmusic.cpp Playing music via DirectMusic. */

#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT_2

#define INITGUID
#include "../stdafx.h"
#ifdef WIN32_LEAN_AND_MEAN
	#undef WIN32_LEAN_AND_MEAN // Don't exclude rarely-used stuff from Windows headers
#endif
#include "../debug.h"
#include "../os/windows/win32.h"
#include "dmusic.h"

#include <windows.h>
#include <dmksctrl.h>
#include <dmusici.h>
#include <dmusicc.h>
#include <dmusicf.h>

static FMusicDriver_DMusic iFMusicDriver_DMusic;

/** the performance object controls manipulation of the segments */
static IDirectMusicPerformance *performance = NULL;

/** the loader object can load many types of DMusic related files */
static IDirectMusicLoader *loader = NULL;

/** the segment object is where the MIDI data is stored for playback */
static IDirectMusicSegment *segment = NULL;

static bool seeking = false;


#define M(x) x "\0"
static const char ole_files[] =
	M("ole32.dll")
	M("CoCreateInstance")
	M("CoInitialize")
	M("CoUninitialize")
	M("")
;
#undef M

struct ProcPtrs {
	unsigned long (WINAPI * CoCreateInstance)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv);
	HRESULT (WINAPI * CoInitialize)(LPVOID pvReserved);
	void (WINAPI * CoUninitialize)();
};

static ProcPtrs proc;


const char *MusicDriver_DMusic::Start(const char * const *parm)
{
	if (performance != NULL) return NULL;

	if (proc.CoCreateInstance == NULL) {
		if (!LoadLibraryList((Function*)&proc, ole_files)) {
			return "ole32.dll load failed";
		}
	}

	/* Initialize COM */
	if (FAILED(proc.CoInitialize(NULL))) {
		return "COM initialization failed";
	}

	/* create the performance object */
	if (FAILED(proc.CoCreateInstance(
				CLSID_DirectMusicPerformance,
				NULL,
				CLSCTX_INPROC,
				IID_IDirectMusicPerformance,
				(LPVOID*)&performance
			))) {
		proc.CoUninitialize();
		return "Failed to create the performance object";
	}

	/* initialize it */
	if (FAILED(performance->Init(NULL, NULL, NULL))) {
		performance->Release();
		performance = NULL;
		proc.CoUninitialize();
		return "Failed to initialize performance object";
	}

	/* choose default Windows synth */
	if (FAILED(performance->AddPort(NULL))) {
		performance->CloseDown();
		performance->Release();
		performance = NULL;
		proc.CoUninitialize();
		return "AddPort failed";
	}

	/* create the loader object; this will be used to load the MIDI file */
	if (FAILED(proc.CoCreateInstance(
				CLSID_DirectMusicLoader,
				NULL,
				CLSCTX_INPROC,
				IID_IDirectMusicLoader,
				(LPVOID*)&loader
			))) {
		performance->CloseDown();
		performance->Release();
		performance = NULL;
		proc.CoUninitialize();
		return "Failed to create loader object";
	}

	return NULL;
}


void MusicDriver_DMusic::Stop()
{
	seeking = false;

	if (performance != NULL) performance->Stop(NULL, NULL, 0, 0);

	if (segment != NULL) {
		segment->SetParam(GUID_Unload, 0xFFFFFFFF, 0, 0, performance);
		segment->Release();
		segment = NULL;
	}

	if (performance != NULL) {
		performance->CloseDown();
		performance->Release();
		performance = NULL;
	}

	if (loader != NULL) {
		loader->Release();
		loader = NULL;
	}

	proc.CoUninitialize();
}


void MusicDriver_DMusic::PlaySong(const char *filename)
{
	/* set up the loader object info */
	DMUS_OBJECTDESC obj_desc;
	ZeroMemory(&obj_desc, sizeof(obj_desc));
	obj_desc.dwSize = sizeof(obj_desc);
	obj_desc.guidClass = CLSID_DirectMusicSegment;
	obj_desc.dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME | DMUS_OBJ_FULLPATH;
	MultiByteToWideChar(
		CP_ACP, MB_PRECOMPOSED,
		filename, -1,
		obj_desc.wszFileName, lengthof(obj_desc.wszFileName)
	);

	/* release the existing segment if we have any */
	if (segment != NULL) {
		segment->Release();
		segment = NULL;
	}

	/* make a new segment */
	if (FAILED(loader->GetObject(
				&obj_desc, IID_IDirectMusicSegment, (LPVOID*)&segment
			))) {
		DEBUG(driver, 0, "DirectMusic: GetObject failed");
		return;
	}

	/* tell the segment what kind of data it contains */
	if (FAILED(segment->SetParam(
				GUID_StandardMIDIFile, 0xFFFFFFFF, 0, 0, performance
			))) {
		DEBUG(driver, 0, "DirectMusic: SetParam (MIDI file) failed");
		return;
	}

	/* tell the segment to 'download' the instruments */
	if (FAILED(segment->SetParam(GUID_Download, 0xFFFFFFFF, 0, 0, performance))) {
		DEBUG(driver, 0, "DirectMusic: failed to download instruments");
		return;
	}

	/* start playing the MIDI file */
	if (FAILED(performance->PlaySegment(segment, 0, 0, NULL))) {
		DEBUG(driver, 0, "DirectMusic: PlaySegment failed");
		return;
	}

	seeking = true;
}


void MusicDriver_DMusic::StopSong()
{
	if (FAILED(performance->Stop(segment, NULL, 0, 0))) {
		DEBUG(driver, 0, "DirectMusic: StopSegment failed");
	}
	seeking = false;
}


bool MusicDriver_DMusic::IsSongPlaying()
{
	/* Not the nicest code, but there is a short delay before playing actually
	 * starts. OpenTTD makes no provision for this. */
	if (performance->IsPlaying(segment, NULL) == S_OK) {
		seeking = false;
		return true;
	} else {
		return seeking;
	}
}


void MusicDriver_DMusic::SetVolume(byte vol)
{
	long db = vol * 2000 / 127 - 2000; ///< 0 - 127 -> -2000 - 0
	performance->SetGlobalParam(GUID_PerfMasterVolume, &db, sizeof(db));
}


#endif /* WIN32_ENABLE_DIRECTMUSIC_SUPPORT */
