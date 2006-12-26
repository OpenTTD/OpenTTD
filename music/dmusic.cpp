/* $Id$ */

#include "../stdafx.h"

#ifdef WIN32_ENABLE_DIRECTMUSIC_SUPPORT

extern "C" {
	#include "../openttd.h"
	#include "../debug.h"
	#include "../win32.h"
	#include "dmusic.h"
}

#include <windows.h>
#include <dmksctrl.h>
#include <dmusici.h>
#include <dmusicc.h>
#include <dmusicf.h>


// the performance object controls manipulation of the segments
static IDirectMusicPerformance* performance = NULL;

// the loader object can load many types of DMusic related files
static IDirectMusicLoader* loader = NULL;

// the segment object is where the MIDI data is stored for playback
static IDirectMusicSegment* segment = NULL;

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
	unsigned long (WINAPI * CoCreateInstance)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);
	HRESULT (WINAPI * CoInitialize)(LPVOID pvReserved);
	void (WINAPI * CoUninitialize)();
};

static ProcPtrs proc;


static const char* DMusicMidiStart(const char* const* parm)
{
	if (performance != NULL) return NULL;

	if (proc.CoCreateInstance == NULL) {
		if (!LoadLibraryList((Function*)&proc, ole_files))
			return "ole32.dll load failed";
	}

	// Initialize COM
	if (FAILED(proc.CoInitialize(NULL))) {
		return "COM initialization failed";
	}

	// create the performance object
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

	// initialize it
	if (FAILED(performance->Init(NULL, NULL, NULL))) {
		performance->Release();
		performance = NULL;
		proc.CoUninitialize();
		return "Failed to initialize performance object";
	}

	// choose default Windows synth
	if (FAILED(performance->AddPort(NULL))) {
		performance->CloseDown();
		performance->Release();
		performance = NULL;
		proc.CoUninitialize();
		return "AddPort failed";
	}

	// create the loader object; this will be used to load the MIDI file
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


static void DMusicMidiStop(void)
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


static void DMusicMidiPlaySong(const char* filename)
{
	// set up the loader object info
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

	// release the existing segment if we have any
	if (segment != NULL) {
		segment->Release();
		segment = NULL;
	}

	// make a new segment
	if (FAILED(loader->GetObject(
				&obj_desc, IID_IDirectMusicSegment, (LPVOID*)&segment
			))) {
		DEBUG(driver, 0, "DirectMusic: GetObject failed");
		return;
	}

	// tell the segment what kind of data it contains
	if (FAILED(segment->SetParam(
				GUID_StandardMIDIFile, 0xFFFFFFFF, 0, 0, performance
			))) {
		DEBUG(driver, 0, "DirectMusic: SetParam (MIDI file) failed");
		return;
	}

	// tell the segment to 'download' the instruments
	if (FAILED(segment->SetParam(GUID_Download, 0xFFFFFFFF, 0, 0, performance))) {
		DEBUG(driver, 0, "DirectMusic: failed to download instruments");
		return;
	}

	// start playing the MIDI file
	if (FAILED(performance->PlaySegment(segment, 0, 0, NULL))) {
		DEBUG(driver, 0, "DirectMusic: PlaySegment failed");
		return;
	}

	seeking = true;
}


static void DMusicMidiStopSong(void)
{
	if (FAILED(performance->Stop(segment, NULL, 0, 0))) {
		DEBUG(driver, 0, "DirectMusic: StopSegment failed");
	}
	seeking = false;
}


static bool DMusicMidiIsSongPlaying(void)
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


static void DMusicMidiSetVolume(byte vol)
{
	// 0 - 127 -> -2000 - 0
	long db = vol * 2000 / 127 - 2000;
	performance->SetGlobalParam(GUID_PerfMasterVolume, &db, sizeof(db));
}


extern "C" const HalMusicDriver _dmusic_midi_driver = {
	DMusicMidiStart,
	DMusicMidiStop,
	DMusicMidiPlaySong,
	DMusicMidiStopSong,
	DMusicMidiIsSongPlaying,
	DMusicMidiSetVolume,
};

#endif
