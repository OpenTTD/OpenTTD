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
#undef FACILITY_DIRECTMUSIC // Needed for newer Windows SDK version.
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


/**
 * Adjust the volume of a playing MIDI file
 */
class VolumeControlTool : public IDirectMusicTool {
public:
	/* IUnknown */
	STDMETHOD(QueryInterface) (THIS_ REFIID iid, LPVOID FAR *outptr)
	{
		if (iid == IID_IUnknown) {
			*outptr = (IUnknown*)this;
		} else if (iid == IID_IDirectMusicTool) {
			*outptr = (IDirectMusicTool*)this;
		} else {
			*outptr = NULL;
		}
		if (*outptr == NULL) {
			return E_NOINTERFACE;
		} else {
			return S_OK;
		}
	}

	STDMETHOD_(ULONG, AddRef) (THIS)
	{
		/* class is only used as a single static instance, doesn't need refcounting */
		return 1;
	}

	STDMETHOD_(ULONG, Release) (THIS)
	{
		/* class is only used as a single static instance, doesn't need refcounting */
		return 1;
	}

	/*  IDirectMusicTool */
	STDMETHOD(Init) (THIS_ IDirectMusicGraph* pGraph)
	{
		return S_OK;
	}
	STDMETHOD(GetMsgDeliveryType) (THIS_ DWORD* pdwDeliveryType)
	{
		*pdwDeliveryType = DMUS_PMSGF_TOOL_QUEUE;
		return S_OK;
	}

	STDMETHOD(GetMediaTypeArraySize) (THIS_ DWORD* pdwNumElements)
	{
		*pdwNumElements = 1;
		return S_OK;
	}

	STDMETHOD(GetMediaTypes) (THIS_ DWORD** padwMediaTypes, DWORD dwNumElements)
	{
		if (dwNumElements < 1) return S_FALSE;
		*padwMediaTypes[0] = DMUS_PMSGT_MIDI;
		return S_OK;
	}

	STDMETHOD(ProcessPMsg) (THIS_ IDirectMusicPerformance* pPerf, DMUS_PMSG* pPMSG)
	{
		pPMSG->pGraph->StampPMsg(pPMSG);
		if (pPMSG->dwType == DMUS_PMSGT_MIDI) {
			DMUS_MIDI_PMSG *msg = (DMUS_MIDI_PMSG*)pPMSG;
			if ((msg->bStatus & 0xF0) == 0xB0) {
				/* controller change message */
				byte channel = msg->dwPChannel & 0x0F; /* technically wrong, but seems to hold for standard midi files */
				if (msg->bByte1 == 0x07) {
					/* main volume for channel */
					if (msg->punkUser == this) {
						/* if the user pointer is set to 'this', it's a sentinel message for user volume control change.
						 * in that case don't store, but just send the actual current adjusted channel volume  */
						msg->punkUser = NULL;
					} else {
						this->current_controllers[channel] = msg->bByte2;
						this->CalculateAdjustedControllers();
						DEBUG(driver, 2, "DirectMusic: song volume adjust ch=%2d  before=%3d  after=%3d  (pch=%08x)", (int)channel, (int)msg->bByte2, (int)this->adjusted_controllers[channel], msg->dwPChannel);
					}
					msg->bByte2 = this->adjusted_controllers[channel];
				} else if (msg->bByte1 == 0x79) {
					/* reset all controllers */
					this->current_controllers[channel] = 127;
					this->adjusted_controllers[channel] = this->current_volume;
				}
			}
		}
		return DMUS_S_REQUEUE;
	}

	STDMETHOD(Flush) (THIS_ IDirectMusicPerformance* pPerf, DMUS_PMSG* pPMSG, REFERENCE_TIME rtTime)
	{
		return DMUS_S_REQUEUE;
	}

private:
	byte current_volume;
	byte current_controllers[16];
	byte adjusted_controllers[16];

	void CalculateAdjustedControllers()
	{
		for (int ch = 0; ch < 16; ch++) {
			this->adjusted_controllers[ch] = this->current_controllers[ch] * this->current_volume / 127;
		}
	}

public:
	static VolumeControlTool instance;

	VolumeControlTool()
	{
		this->current_volume = 127;
		for (int ch = 0; ch < 16; ch++) {
			this->current_controllers[ch] = 127;
			this->adjusted_controllers[ch] = 127;
		}
	}

	void SetVolume(byte new_volume)
	{
		/* update volume values */
		this->current_volume = new_volume;
		this->CalculateAdjustedControllers();

		if (performance == NULL) return;

		DEBUG(driver, 2, "DirectMusic: user adjust volume, new adjusted values = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			this->adjusted_controllers[ 0], this->adjusted_controllers[ 1], this->adjusted_controllers[ 2], this->adjusted_controllers[ 3],
			this->adjusted_controllers[ 4], this->adjusted_controllers[ 5], this->adjusted_controllers[ 6], this->adjusted_controllers[ 7],
			this->adjusted_controllers[ 8], this->adjusted_controllers[ 9], this->adjusted_controllers[10], this->adjusted_controllers[11],
			this->adjusted_controllers[12], this->adjusted_controllers[13], this->adjusted_controllers[14], this->adjusted_controllers[15]
			);

		/* send volume change messages to all channels for instant update */
		IDirectMusicGraph *graph = NULL;
		if (FAILED(performance->QueryInterface(IID_IDirectMusicGraph, (LPVOID*)&graph))) return;

		MUSIC_TIME time = 0;
		performance->GetTime(NULL, &time);

		for (int ch = 0; ch < 16; ch++) {
			DMUS_MIDI_PMSG *msg = NULL;
			if (SUCCEEDED(performance->AllocPMsg(sizeof(*msg), (DMUS_PMSG**)&msg))) {
				memset(msg, 0, sizeof(*msg));
				msg->dwSize = sizeof(*msg);
				msg->dwType = DMUS_PMSGT_MIDI;
				msg->punkUser = this; /* sentinel to indicate this message is to update playback volume, not part of the original song */
				msg->dwFlags = DMUS_PMSGF_MUSICTIME;
				msg->dwPChannel = ch; /* technically wrong, but DirectMusic doesn't have a way to obtain PChannel number given a MIDI channel, you just have to know it */
				msg->mtTime = time;
				msg->bStatus = 0xB0 | ch; /* controller change for channel ch */
				msg->bByte1 = 0x07; /* channel volume controller */
				msg->bByte2 = this->adjusted_controllers[ch];
				graph->StampPMsg((DMUS_PMSG*)msg);
				if (FAILED(performance->SendPMsg((DMUS_PMSG*)msg))) {
					performance->FreePMsg((DMUS_PMSG*)msg);
				}
			}
		}

		graph->Release();
	}
};

/** Static instance of the volume control tool */
VolumeControlTool VolumeControlTool::instance;


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

	IDirectMusicGraph *graph = NULL;
	if (FAILED(proc.CoCreateInstance(
				CLSID_DirectMusicGraph,
				NULL,
				CLSCTX_INPROC,
				IID_IDirectMusicGraph,
				(LPVOID*)&graph
		))) {
		return "Failed to create the graph object";
	}
	graph->InsertTool(&VolumeControlTool::instance, NULL, 0, 0);
	performance->SetGraph(graph);
	graph->Release();

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

	if (performance != NULL) {
		performance->Stop(NULL, NULL, 0, 0);
		/* necessary to sleep, otherwise note-off messages aren't always sent to the output device
		 * and can leave notes hanging on external synths, in particular during game shutdown */
		Sleep(100);
	}

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
	VolumeControlTool::instance.SetVolume(vol);
}


#endif /* WIN32_ENABLE_DIRECTMUSIC_SUPPORT */
