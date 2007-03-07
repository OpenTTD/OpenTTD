/* $Id$ */

#ifndef NEWGRF_SOUND_H
#define NEWGRF_SOUND_H

enum VehicleSoundEvent {
	VSE_START        = 1,
	VSE_TUNNEL       = 2,
	VSE_BREAKDOWN    = 3,
	VSE_RUNNING      = 4,
	VSE_TOUCHDOWN    = 5,
	VSE_TRAIN_EFFECT = 6,
	VSE_RUNNING_16   = 7,
	VSE_STOPPED_16   = 8,
	VSE_LOAD_UNLOAD  = 9,
};


FileEntry *AllocateFileEntry();
void InitializeSoundPool();
FileEntry *GetSound(uint index);
uint GetNumSounds();
bool PlayVehicleSound(const Vehicle *v, VehicleSoundEvent event);

#endif /* NEWGRF_SOUND_H */
