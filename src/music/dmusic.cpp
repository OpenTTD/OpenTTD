/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dmusic.cpp Playing music via DirectMusic. */

#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT

#define INITGUID
#include "../stdafx.h"
#ifdef WIN32_LEAN_AND_MEAN
	#undef WIN32_LEAN_AND_MEAN // Don't exclude rarely-used stuff from Windows headers
#endif
#include "../debug.h"
#include "../os/windows/win32.h"
#include "../core/mem_func.hpp"
#include "dmusic.h"

#include <windows.h>
#include <dmksctrl.h>
#include <dmusici.h>
#include <dmusicc.h>
#include <dmusicf.h>

#include "../safeguards.h"

static FMusicDriver_DMusic iFMusicDriver_DMusic;

/** the direct music object manages buffers and ports */
static IDirectMusic *music = NULL;

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
		return "Failed to create the performance object";
	}

	/* initialize it */
	if (FAILED(performance->Init(&music, NULL, NULL))) {
		return "Failed to initialize performance object";
	}

	int port = GetDriverParamInt(parm, "port", -1);

#ifndef NO_DEBUG_MESSAGES
	if (_debug_driver_level > 0) {
		/* Print all valid output ports. */
		char desc[DMUS_MAX_DESCRIPTION];

		DMUS_PORTCAPS caps;
		MemSetT(&caps, 0);
		caps.dwSize = sizeof(DMUS_PORTCAPS);

		DEBUG(driver, 1, "Detected DirectMusic ports:");
		for (int i = 0; music->EnumPort(i, &caps) == S_OK; i++) {
			if (caps.dwClass == DMUS_PC_OUTPUTCLASS) {
				/* Description is UNICODE even for ANSI build. */
				DEBUG(driver, 1, " %d: %s%s", i, convert_from_fs(caps.wszDescription, desc, lengthof(desc)), i == port ? " (selected)" : "");
			}
		}
	}
#endif

	IDirectMusicPort *music_port = NULL; // NULL means 'use default port'.

	if (port >= 0) {
		/* Check if the passed port is a valid port. */
		DMUS_PORTCAPS caps;
		MemSetT(&caps, 0);
		caps.dwSize = sizeof(DMUS_PORTCAPS);
		if (FAILED(music->EnumPort(port, &caps))) return "Supplied port parameter is not a valid port";
		if (caps.dwClass != DMUS_PC_OUTPUTCLASS) return "Supplied port parameter is not an output port";

		/* Create new port. */
		DMUS_PORTPARAMS params;
		MemSetT(&params, 0);
		params.dwSize          = sizeof(DMUS_PORTPARAMS);
		params.dwValidParams   = DMUS_PORTPARAMS_CHANNELGROUPS;
		params.dwChannelGroups = 1;

		if (FAILED(music->CreatePort(caps.guidPort, &params, &music_port, NULL))) {
			return "Failed to create port";
		}

		/* Activate port. */
		if (FAILED(music_port->Activate(TRUE))) {
			music_port->Release();
			return "Failed to activate port";
		}
	}

	/* Add port to performance. */
	if (FAILED(performance->AddPort(music_port))) {
		if (music_port != NULL) music_port->Release();
		return "AddPort failed";
	}

	/* Assign a performance channel block to the performance if we added
	 * a custom port to the performance. */
	if (music_port != NULL) {
		if (FAILED(performance->AssignPChannelBlock(0, music_port, 1))) {
			music_port->Release();
			return "Failed to assign PChannel block";
		}
		/* We don't need the port anymore. */
		music_port->Release();
	}

	/* create the loader object; this will be used to load the MIDI file */
	if (FAILED(proc.CoCreateInstance(
				CLSID_DirectMusicLoader,
				NULL,
				CLSCTX_INPROC,
				IID_IDirectMusicLoader,
				(LPVOID*)&loader
			))) {
		return "Failed to create loader object";
	}

	return NULL;
}


MusicDriver_DMusic::~MusicDriver_DMusic()
{
	this->Stop();
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

	if (music != NULL) {
		music->Release();
		music = NULL;
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
