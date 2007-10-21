/* $Id$ */

/*****************************************************************************
 *                             Cocoa sound driver                            *
 * Known things left to do:                                                  *
 * - Might need to do endian checking for it to work on both ppc and x86     *
 *****************************************************************************/

#ifdef WITH_COCOA

#include <AudioUnit/AudioUnit.h>

/* Name conflict */
#define Rect        OTTDRect
#define Point       OTTDPoint
#define WindowClass OTTDWindowClass

#include "../stdafx.h"
#include "../debug.h"
#include "../driver.h"
#include "../mixer.h"

#include "cocoa_s.h"

#undef WindowClass
#undef Point
#undef Rect

static FSoundDriver_Cocoa iFSoundDriver_Cocoa;

static AudioUnit _outputAudioUnit;

/* The CoreAudio callback */
static OSStatus audioCallback(void *inRefCon, AudioUnitRenderActionFlags inActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, AudioBuffer *ioData)
{
	MxMixSamples(ioData->mData, ioData->mDataByteSize / 4);

	return noErr;
}


const char *SoundDriver_Cocoa::Start(const char * const *parm)
{
	Component comp;
	ComponentDescription desc;
	struct AudioUnitInputCallback callback;
	AudioStreamBasicDescription requestedDesc;

	/* Setup a AudioStreamBasicDescription with the requested format */
	requestedDesc.mFormatID = kAudioFormatLinearPCM;
	requestedDesc.mFormatFlags = kLinearPCMFormatFlagIsPacked;
	requestedDesc.mChannelsPerFrame = 2;
	requestedDesc.mSampleRate = GetDriverParamInt(parm, "hz", 11025);

	requestedDesc.mBitsPerChannel = 16;
	requestedDesc.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;

#ifdef TTD_BIG_ENDIAN
	requestedDesc.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif

	requestedDesc.mFramesPerPacket = 1;
	requestedDesc.mBytesPerFrame = requestedDesc.mBitsPerChannel * requestedDesc.mChannelsPerFrame / 8;
	requestedDesc.mBytesPerPacket = requestedDesc.mBytesPerFrame * requestedDesc.mFramesPerPacket;


	/* Locate the default output audio unit */
	desc.componentType = kAudioUnitComponentType;
	desc.componentSubType = kAudioUnitSubType_Output;
	desc.componentManufacturer = kAudioUnitID_DefaultOutput;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent (NULL, &desc);
	if (comp == NULL) {
		return "cocoa_s: Failed to start CoreAudio: FindNextComponent returned NULL";
	}

	/* Open & initialize the default output audio unit */
	if (OpenAComponent(comp, &_outputAudioUnit) != noErr) {
		return "cocoa_s: Failed to start CoreAudio: OpenAComponent";
	}

	if (AudioUnitInitialize(_outputAudioUnit) != noErr) {
		return "cocoa_s: Failed to start CoreAudio: AudioUnitInitialize";
	}

	/* Set the input format of the audio unit. */
	if (AudioUnitSetProperty(_outputAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &requestedDesc, sizeof(requestedDesc)) != noErr) {
		return "cocoa_s: Failed to start CoreAudio: AudioUnitSetProperty (kAudioUnitProperty_StreamFormat)";
	}

	/* Set the audio callback */
	callback.inputProc = audioCallback;
	callback.inputProcRefCon = NULL;
	if (AudioUnitSetProperty(_outputAudioUnit, kAudioUnitProperty_SetInputCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback)) != noErr) {
		return "cocoa_s: Failed to start CoreAudio: AudioUnitSetProperty (kAudioUnitProperty_SetInputCallback)";
	}

	/* Finally, start processing of the audio unit */
	if (AudioOutputUnitStart(_outputAudioUnit) != noErr) {
		return "cocoa_s: Failed to start CoreAudio: AudioOutputUnitStart";
	}

	/* We're running! */
	return NULL;
}


void SoundDriver_Cocoa::Stop()
{
	struct AudioUnitInputCallback callback;

	/* stop processing the audio unit */
	if (AudioOutputUnitStop(_outputAudioUnit) != noErr) {
		DEBUG(driver, 0, "cocoa_s: Core_CloseAudio: AudioOutputUnitStop failed");
		return;
	}

	/* Remove the input callback */
	callback.inputProc = 0;
	callback.inputProcRefCon = 0;
	if (AudioUnitSetProperty(_outputAudioUnit, kAudioUnitProperty_SetInputCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback)) != noErr) {
		DEBUG(driver, 0, "cocoa_s: Core_CloseAudio: AudioUnitSetProperty (kAudioUnitProperty_SetInputCallback) failed");
		return;
	}

	if (CloseComponent(_outputAudioUnit) != noErr) {
		DEBUG(driver, 0, "cocoa_s: Core_CloseAudio: CloseComponent failed");
		return;
	}
}

#endif /* WITH_COCOA */
