#include "stdafx.h"
#include "ttd.h"
#include "map.h"
#include "sound.h"
#include "vehicle.h"
#include "window.h"
#include "viewport.h"
#include "fileio.h"

typedef struct MixerChannel {
	// Mixer
	Mixer *mx;
	bool active;

	// pointer to allocated buffer memory
	int8 *memory;

	// current position in memory
	uint32 pos;
	uint32 frac_pos;
	uint32 frac_speed;
	uint32 samples_left;

	// Mixing volume
	uint volume_left;
	uint volume_right;

	uint flags;
} MixerChannel;

typedef struct FileEntry {
	uint32 file_offset;
	uint32 file_size;
	uint16 rate;
	uint8 bits_per_sample;
	uint8 channels;
} FileEntry;

struct Mixer {
	uint32 play_rate;
	FileEntry *files;
	uint32 file_count;
	MixerChannel channels[8];
};

enum {
	MX_AUTOFREE = 1,
//	MX_8BIT = 2,
//	MX_STEREO = 4,
	MX_UNSIGNED = 8,
};

#define SOUND_SLOT 31


static void mix_int8_to_int16(MixerChannel *sc, int16 *buffer, uint samples)
{
	int8 *b;
	uint32 frac_pos;
	uint32 frac_speed;
	uint volume_left;
	uint volume_right;

	if (samples > sc->samples_left) samples = sc->samples_left;
	sc->samples_left -= samples;
	assert(samples > 0);

	b = sc->memory + sc->pos;
	frac_pos = sc->frac_pos;
	frac_speed = sc->frac_speed;
	volume_left = sc->volume_left;
	volume_right = sc->volume_right;

	if (frac_speed == 0x10000) {
		// Special case when frac_speed is 0x10000
		do {
			buffer[0] += *b * volume_left >> 8;
			buffer[1] += *b * volume_right >> 8;
			b++;
			buffer += 2;
		} while (--samples > 0);
	} else {
		do {
			buffer[0] += *b * volume_left >> 8;
			buffer[1] += *b * volume_right >> 8;
			buffer += 2;
			frac_pos += frac_speed;
			b += frac_pos >> 16;
			frac_pos &= 0xffff;
		} while (--samples > 0);
	}

	sc->frac_pos = frac_pos;
	sc->pos = b - sc->memory;
}

static void MxCloseChannel(MixerChannel *mc)
{
	if (mc->flags & MX_AUTOFREE) free(mc->memory);
	mc->memory = NULL;
	mc->active = false;
}

void MxMixSamples(Mixer *mx, void *buffer, uint samples)
{
	MixerChannel *mc;

	// Clear the buffer
	memset(buffer, 0, sizeof(int16) * 2 * samples);

	// Mix each channel
	for (mc = mx->channels; mc != endof(mx->channels); mc++) {
		if (mc->active) {
			mix_int8_to_int16(mc, buffer, samples);
			if (mc->samples_left == 0) MxCloseChannel(mc);
		}
	}

	#if 0
	{
		static FILE *out = NULL;
		if (out == NULL)
			out = fopen("d:\\dump.raw", "wb");
		fwrite(buffer, samples * 4, 1, out);
	}
	#endif
}

static MixerChannel *MxAllocateChannel(Mixer *mx)
{
	MixerChannel *mc;
	for (mc = mx->channels; mc != endof(mx->channels); mc++)
		if (mc->memory == NULL) {
			mc->active = false;
			mc->mx = mx;
			return mc;
		}
	return NULL;
}

static void MxSetChannelRawSrc(MixerChannel *mc, int8 *mem, uint size, uint rate, uint flags)
{
	mc->memory = mem;
	mc->flags = flags;
	mc->frac_pos = 0;
	mc->pos = 0;

	mc->frac_speed = (rate << 16) / mc->mx->play_rate;

	// adjust the magnitude to prevent overflow
	while (size & 0xFFFF0000) {
		size >>= 1;
		rate = (rate >> 1) + 1;
	}

	mc->samples_left = size * mc->mx->play_rate / rate;
}

static void MxSetChannelVolume(MixerChannel *mc, uint left, uint right)
{
	mc->volume_left = left;
	mc->volume_right = right;
}

static void MxOpenBankFile(Mixer *mx, const char *filename)
{
	uint count, i;
	uint32 size, tag;
	FileEntry *fe;

	FioOpenFile(SOUND_SLOT, filename);
	mx->file_count = count = FioReadDword() >> 3;
	fe = mx->files = calloc(sizeof(FileEntry), count);

	FioSeekTo(0, SEEK_SET);

	for (i = 0; i != count; i++, fe++) {
		fe->file_offset = FioReadDword();
		fe->file_size = FioReadDword();
	}

	fe = mx->files;
	for (i = 0; i != count; i++, fe++) {
		char name[255];

		FioSeekTo(fe->file_offset, SEEK_SET);

		// Check for special case, see else case
		FioReadBlock(name, FioReadByte()); // Read the name of the sound
		if (strcmp(name, "Corrupt sound") != 0) {
			FioSeekTo(12, SEEK_CUR); // Skip past RIFF header

			// Read riff tags
			for (;;) {
				tag = FioReadDword();
				size = FioReadDword();

				if (tag == ' tmf') {
					FioReadWord(); // wFormatTag
					fe->channels = FioReadWord(); // wChannels
					FioReadDword();   // samples per second
					fe->rate = 11025; // seems like all samples should be played at this rate.
					FioReadDword();   // avg bytes per second
					FioReadWord();    // alignment
					fe->bits_per_sample = FioReadByte(); // bits per sample
					FioSeekTo(size - (2 + 2 + 4 + 4 + 2 + 1), SEEK_CUR);
				} else if (tag == 'atad') {
					fe->file_size = size;
					fe->file_offset = FioGetPos() | (SOUND_SLOT << 24);
					break;
				} else {
					fe->file_size = 0;
					break;
				}
			}
		} else {
			/*
			 * Special case for the jackhammer sound
			 * (name in sample.cat is "Corrupt sound")
			 * It's no RIFF file, but raw PCM data
			 */
			fe->channels = 1;
			fe->rate = 11025;
			fe->bits_per_sample = 8;
			fe->file_offset = FioGetPos() | (SOUND_SLOT << 24);
		}
	}
}

static bool MxSetBankSource(MixerChannel *mc, uint bank)
{
	FileEntry *fe = &mc->mx->files[bank];
	int8 *mem;
	uint i;

	if (fe->file_size == 0)
		return false;

	mem = malloc(fe->file_size); /* XXX unchecked malloc */
	FioSeekToFile(fe->file_offset);
	FioReadBlock(mem, fe->file_size);

	for (i = 0; i != fe->file_size; i++)
		mem[i] -= 128; // Convert unsigned sound data to signed

	assert(fe->bits_per_sample == 8 && fe->channels == 1 && fe->file_size != 0 && fe->rate != 0);

	MxSetChannelRawSrc(mc, mem, fe->file_size, fe->rate, MX_AUTOFREE | MX_UNSIGNED);

	return true;
}

bool MxInitialize(uint rate, const char *filename)
{
	static Mixer mx;
	_mixer = &mx;
	mx.play_rate = rate;
	MxOpenBankFile(&mx, filename);
	return true;
}

// Low level sound player
static void StartSound(uint sound, uint panning, uint volume)
{
	if (volume != 0) {
		MixerChannel *mc = MxAllocateChannel(_mixer);
		if (mc == NULL)
			return;
		if (MxSetBankSource(mc, sound)) {
			MxSetChannelVolume(mc, volume << 8, volume << 8);
			mc->active = true;
		}
	}
}


static const byte _vol_factor_by_zoom[] = {255, 190, 134};

static const byte _sound_base_vol[] = {
	128,  90, 128, 128, 128, 128, 128, 128,
	128,  90,  90, 128, 128, 128, 128, 128,
	128, 128, 128,  80, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128,
	128, 128,  90,  90,  90, 128,  90, 128,
	128,  90, 128, 128, 128,  90, 128, 128,
	128, 128, 128, 128,  90, 128, 128, 128,
	128,  90, 128, 128, 128, 128, 128, 128,
	128, 128,  90,  90,  90, 128, 128, 128,
	 90,
};

static const byte _sound_idx[] = {
	2, 3, 4, 5, 6, 7, 8, 9,
	10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25,
	26, 27, 28, 29, 30, 31, 32, 33,
	34, 35, 36, 37, 38, 39, 40, 0,
	1, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71,
	72,
};

static void SndPlayScreenCoordFx(SoundFx sound, int x, int y)
{
	Window *w;
	ViewPort *vp;
	int left;

	if (msf.effect_vol == 0)
		return;

	for (w = _windows; w != _last_window; w++) {
		if ((vp = w->viewport) != NULL &&
				IS_INSIDE_1D(x, vp->virtual_left, vp->virtual_width) &&
				IS_INSIDE_1D(y, vp->virtual_top, vp->virtual_height)) {

			left = ((x - vp->virtual_left) >> vp->zoom) + vp->left;
			StartSound(
				_sound_idx[sound],
				clamp(left / 71, 0, 8),
				(_sound_base_vol[sound] * msf.effect_vol * _vol_factor_by_zoom[vp->zoom]) >> 15
			);
			return;
		}
	}

}

void SndPlayTileFx(SoundFx sound, TileIndex tile)
{
	/* emits sound from center (+ 8) of the tile */
	int x = GET_TILE_X(tile) * 16 + 8;
	int y = GET_TILE_Y(tile) * 16 + 8;
	Point pt = RemapCoords(x, y, GetSlopeZ(x, y));
	SndPlayScreenCoordFx(sound, pt.x, pt.y);
}

void SndPlayVehicleFx(SoundFx sound, const Vehicle *v)
{
	SndPlayScreenCoordFx(sound,
		(v->left_coord + v->right_coord) / 2,
		(v->top_coord + v->bottom_coord) / 2
	);
}

void SndPlayFx(SoundFx sound)
{
	StartSound(
		_sound_idx[sound],
		4,
		(_sound_base_vol[sound] * msf.effect_vol) >> 7
	);
}
