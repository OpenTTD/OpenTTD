/*********************************************************************
 * OpenTTD: An Open Source Transport Tycoon Deluxe clone             *
 * Copyright (c) 2002-2004 OpenTTD Developers. All Rights Reserved.  *
 *                                                                   *
 * Web site: http://openttd.sourceforge.net/                         *
 *********************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* DirectMusic driver for Win32 */
/* Based on dxmci from TTDPatch */

#include "stdafx.h"

#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT

// for gcc, the GUIDs are available in a library instead
#ifndef __GNUC__
#define INITGUID
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#include "ttd.h"
#include "sound.h"
#include "hal.h"

#ifdef __cplusplus
	}
#endif

#include <windows.h>
#include <stdio.h>

#include <dmksctrl.h>
#include <dmusici.h>
#include <dmusicc.h>
#include <dmusicf.h>

#define VARIANT int

#define MSGBOX(output)	DEBUG(misc, 0) ("DirectMusic driver: %s\n", output); //MessageBox(NULL, output, "dxmci",MB_OK);
#define MULTI_TO_WIDE( x,y )  MultiByteToWideChar( CP_ACP,MB_PRECOMPOSED, y,-1,x,_MAX_PATH);

// the performance object controls manipulation of  the segments
IDirectMusicPerformance *performance = NULL;

// the segment object is where the MIDI data is stored for playback
IDirectMusicSegment *segment = NULL;

// the loader bject can load many types of DMusic related files
IDirectMusicLoader *loader = NULL;						

// whether we've initialized COM or not (when deciding whether to shut down)
int COMInitialized = 0;


extern "C" bool LoadLibraryList(void **proc, const char *dll);

// Use lazy linking
struct ProcPtrs {
	unsigned long (WINAPI *CoCreateInstance)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID * ppv);
	HRESULT (WINAPI *CoInitialize)(  LPVOID pvReserved );
	void (WINAPI *CoUninitialize)( );
};

#define M(x) x "\0"
static const char ole_files[] = 
	M("ole32.dll")
	M("CoCreateInstance")
	M("CoInitialize")
	M("CoUninitialize")
	M("")
;
#undef M


static ProcPtrs _proc;

static bool LoadOleDLL()
{
	if (_proc.CoCreateInstance != NULL) 
		return true;
	if (!LoadLibraryList((void**)&_proc, ole_files))
		return false;
	return true;
}


#ifdef __cplusplus
extern "C" {
#endif

// Initialize COM and DirectMusic
bool InitDirectMusic (void)
{
	if (NULL != performance)
		return true;

	// Initialize COM
	if (!COMInitialized) {
		if (!LoadOleDLL()) {
			MSGBOX("ole32.dll load failed");
			return false;
		}

		_proc.CoInitialize(NULL);
		COMInitialized = 1;
	}

	// Create the performance object via CoCreateInstance
	if (FAILED(_proc.CoCreateInstance(
			(REFCLSID)CLSID_DirectMusicPerformance,
			NULL,
			CLSCTX_INPROC,
			(REFIID)IID_IDirectMusicPerformance,
			(LPVOID *)&performance))) {
		MSGBOX ("Failed to create the performance object");
		return false;
	}

	// Initialize it
	if (FAILED(performance->Init(NULL, NULL, NULL))) {
		MSGBOX("Failed to initialize performance object");
		return false;
	}

	// Choose default Windows synth
	if (FAILED(performance->AddPort (NULL))) {
		MSGBOX("AddPort failed");
		return false;
	}

	// now we'll create the loader object. This will be used to load the
	// midi file for our demo. Again, we need to use CoCreateInstance
	// and pass the appropriate ID parameters
	if (FAILED(_proc.CoCreateInstance((REFCLSID)CLSID_DirectMusicLoader,
			NULL, CLSCTX_INPROC, 
			(REFIID)IID_IDirectMusicLoader,
			(LPVOID *)&loader))) {
		MSGBOX("Failed to create loader object");
		return false;
	}

	// that's it for initialization. If we made it this far we
	// were successful.
	return true;
}

// Releases memory used by all of the initialized 
// DirectMusic objects in the program
void ReleaseSegment (void)
{
	if (NULL != segment) {
		segment->Release ();
		segment = NULL;
	}
}
void ShutdownDirectMusic (void)
{
	// release everything but the segment, which the performance 
	// will release automatically (and it'll crash if it's been 
	// released already)

	if (NULL != loader) {
		loader->Release ();
		loader = NULL;
	}

	if (NULL != performance)
	{
 		performance->CloseDown ();
		performance->Release ();
		performance = NULL;
	}

	if (COMInitialized) {
		_proc.CoUninitialize();
		COMInitialized = 0;
	}
}

// Load MIDI file for playing 
bool LoadMIDI (char *directory, char *filename)
{
	DMUS_OBJECTDESC obj_desc;
	WCHAR w_directory[_MAX_PATH];	// utf-16 version of the directory name.
	WCHAR w_filename[_MAX_PATH];	// utf-16 version of the file name

	if (NULL == performance)
		return false;

	MULTI_TO_WIDE(w_directory,directory);

	if (FAILED(loader->SetSearchDirectory((REFGUID) GUID_DirectMusicAllTypes,
				w_directory, FALSE))) {
		MSGBOX("LoadMIDI: SetSearchDirectory failed");
		return false;
	}

	// set up the loader object info
	ZeroMemory (&obj_desc, sizeof (obj_desc));
	obj_desc.dwSize = sizeof (obj_desc);

	MULTI_TO_WIDE(w_filename,filename);
	obj_desc.guidClass = CLSID_DirectMusicSegment;

	wcscpy (obj_desc.wszFileName,w_filename);
	obj_desc.dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME;

	// release the existing segment if we have any
	if (NULL != segment)
		ReleaseSegment();

	// and make a new segment
	if (FAILED(loader->GetObject(&obj_desc, 
			(REFIID)IID_IDirectMusicSegment, 
			(LPVOID *) &segment))) {
		MSGBOX("LoadMIDI: Get object failed");
		return FALSE;
	}

	// next we need to tell the segment what kind of data it contains. We do this
	// with the IDirectMusicSegment::SetParam function.
	if (FAILED(segment->SetParam((REFGUID)GUID_StandardMIDIFile,
			-1, 0, 0, (LPVOID)performance))) {
		MSGBOX("LoadMIDI: SetParam (MIDI file) failed");
		return false;
	}

	// finally, we need to tell the segment to 'download' the instruments
	if (FAILED(segment->SetParam((REFGUID)GUID_Download,
			-1, 0, 0, (LPVOID)performance))) {
		MSGBOX("LoadMIDI: Failed to download instruments");
		return false;
	}

	// at this point, the MIDI file is loaded and ready to play!
	return true;
}

// Start playing the MIDI file
void PlaySegment (void)
{
	if (NULL == performance)
		return;

	if (FAILED(performance->PlaySegment(segment, 0, 0, NULL))) {
		MSGBOX("PlaySegment failed");
	}
}

// Stop playing
void StopSegment (void)
{
	if (NULL == performance || NULL == segment)
		return;

	if (FAILED(performance->Stop(segment, NULL, 0, 0))) {
		MSGBOX("StopSegment failed");
	}
}

// Find out whether playing has started or stopped
bool IsSegmentPlaying (void)
{
	if (NULL == performance || NULL == segment)
		return FALSE;

	// IsPlaying return S_OK if the segment is currently playing
	return performance->IsPlaying(segment, NULL) == S_OK ? TRUE : FALSE;
}

void SetVolume(long vol)
{
	long db;

	if (performance == NULL && !InitDirectMusic())
		return;

	db = ((vol >> 21) & 0x7ff) - 1000;
	performance->SetGlobalParam(GUID_PerfMasterVolume, &db, sizeof(db));
}

#if defined(__cplusplus)
}
#endif

#endif // WIN32_ENABLE_DIRECTMUSIC_SUPPORT
