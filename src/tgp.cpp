/* $Id$ */

/** @file tgp.cpp OTTD Perlin Noise Landscape Generator, aka TerraGenesis Perlin */

#include "stdafx.h"
#include <math.h>
#include "openttd.h"
#include "clear_map.h"
#include "clear_map.h"
#include "variables.h"
#include "void_map.h"
#include "tgp.h"
#include "genworld.h"
#include "core/alloc_func.hpp"
#include "core/random_func.hpp"
#include "settings_type.h"
#include "landscape_type.h"

#include "table/strings.h"

/*
 *
 * Quickie guide to Perlin Noise
 * Perlin noise is a predictable pseudo random number sequence. By generating
 * it in 2 dimensions, it becomes a useful random map, that for a given seed
 * and starting X & Y is entirely predictable. On the face of it, that may not
 * be useful. However, it means that if you want to replay a map in a different
 * terrain, or just vary the sea level, you just re-run the generator with the
 * same seed. The seed is an int32, and is randomised on each run of New Game.
 * The Scenario Generator does not randomise the value, so that you can
 * experiment with one terrain until you are happy, or click "Random" for a new
 * random seed.
 *
 * Perlin Noise is a series of "octaves" of random noise added together. By
 * reducing the amplitude of the noise with each octave, the first octave of
 * noise defines the main terrain sweep, the next the ripples on that, and the
 * next the ripples on that. I use 6 octaves, with the amplitude controlled by
 * a power ratio, usually known as a persistence or p value. This I vary by the
 * smoothness selection, as can be seen in the table below. The closer to 1,
 * the more of that octave is added. Each octave is however raised to the power
 * of its position in the list, so the last entry in the "smooth" row, 0.35, is
 * raised to the power of 6, so can only add 0.001838...  of the amplitude to
 * the running total.
 *
 * In other words; the first p value sets the general shape of the terrain, the
 * second sets the major variations to that, ... until finally the smallest
 * bumps are added.
 *
 * Usefully, this routine is totally scaleable; so when 32bpp comes along, the
 * terrain can be as bumpy as you like! It is also infinitely expandable; a
 * single random seed terrain continues in X & Y as far as you care to
 * calculate. In theory, we could use just one seed value, but randomly select
 * where in the Perlin XY space we use for the terrain. Personally I prefer
 * using a simple (0, 0) to (X, Y), with a varying seed.
 *
 *
 * Other things i have had to do: mountainous wasnt mountainous enough, and
 * since we only have 0..15 heights available, I add a second generated map
 * (with a modified seed), onto the original. This generally raises the
 * terrain, which then needs scaling back down. Overall effect is a general
 * uplift.
 *
 * However, the values on the top of mountains are then almost guaranteed to go
 * too high, so large flat plateaus appeared at height 15. To counter this, I
 * scale all heights above 12 to proportion up to 15. It still makes the
 * mountains have flatish tops, rather than craggy peaks, but at least they
 * arent smooth as glass.
 *
 *
 * For a full discussion of Perlin Noise, please visit:
 * http://freespace.virgin.net/hugo.elias/models/m_perlin.htm
 *
 *
 * Evolution II
 *
 * The algorithm as described in the above link suggests to compute each tile height
 * as composition of several noise waves. Some of them are computed directly by
 * noise(x, y) function, some are calculated using linear approximation. Our
 * first implementation of perlin_noise_2D() used 4 noise(x, y) calls plus
 * 3 linear interpolations. It was called 6 times for each tile. This was a bit
 * CPU expensive.
 *
 * The following implementation uses optimized algorithm that should produce
 * the same quality result with much less computations, but more memory accesses.
 * The overal speedup should be 300% to 800% depending on CPU and memory speed.
 *
 * I will try to explain it on the example below:
 *
 * Have a map of 4 x 4 tiles, our simplifiead noise generator produces only two
 * values -1 and +1, use 3 octaves with wave lenght 1, 2 and 4, with amplitudes
 * 3, 2, 1. Original algorithm produces:
 *
 * h00 = lerp(lerp(-3, 3, 0/4), lerp(3, -3, 0/4), 0/4) + lerp(lerp(-2,  2, 0/2), lerp( 2, -2, 0/2), 0/2) + -1 = lerp(-3.0,  3.0, 0/4) + lerp(-2,  2, 0/2) + -1 = -3.0  + -2 + -1 = -6.0
 * h01 = lerp(lerp(-3, 3, 1/4), lerp(3, -3, 1/4), 0/4) + lerp(lerp(-2,  2, 1/2), lerp( 2, -2, 1/2), 0/2) +  1 = lerp(-1.5,  1.5, 0/4) + lerp( 0,  0, 0/2) +  1 = -1.5  +  0 +  1 = -0.5
 * h02 = lerp(lerp(-3, 3, 2/4), lerp(3, -3, 2/4), 0/4) + lerp(lerp( 2, -2, 0/2), lerp(-2,  2, 0/2), 0/2) + -1 = lerp(   0,    0, 0/4) + lerp( 2, -2, 0/2) + -1 =    0  +  2 + -1 =  1.0
 * h03 = lerp(lerp(-3, 3, 3/4), lerp(3, -3, 3/4), 0/4) + lerp(lerp( 2, -2, 1/2), lerp(-2,  2, 1/2), 0/2) +  1 = lerp( 1.5, -1.5, 0/4) + lerp( 0,  0, 0/2) +  1 =  1.5  +  0 +  1 =  2.5
 *
 * h10 = lerp(lerp(-3, 3, 0/4), lerp(3, -3, 0/4), 1/4) + lerp(lerp(-2,  2, 0/2), lerp( 2, -2, 0/2), 1/2) +  1 = lerp(-3.0,  3.0, 1/4) + lerp(-2,  2, 1/2) +  1 = -1.5  +  0 +  1 = -0.5
 * h11 = lerp(lerp(-3, 3, 1/4), lerp(3, -3, 1/4), 1/4) + lerp(lerp(-2,  2, 1/2), lerp( 2, -2, 1/2), 1/2) + -1 = lerp(-1.5,  1.5, 1/4) + lerp( 0,  0, 1/2) + -1 = -0.75 +  0 + -1 = -1.75
 * h12 = lerp(lerp(-3, 3, 2/4), lerp(3, -3, 2/4), 1/4) + lerp(lerp( 2, -2, 0/2), lerp(-2,  2, 0/2), 1/2) +  1 = lerp(   0,    0, 1/4) + lerp( 2, -2, 1/2) +  1 =    0  +  0 +  1 =  1.0
 * h13 = lerp(lerp(-3, 3, 3/4), lerp(3, -3, 3/4), 1/4) + lerp(lerp( 2, -2, 1/2), lerp(-2,  2, 1/2), 1/2) + -1 = lerp( 1.5, -1.5, 1/4) + lerp( 0,  0, 1/2) + -1 =  0.75 +  0 + -1 = -0.25
 *
 *
 * Optimization 1:
 *
 * 1) we need to allocate a bit more tiles: (size_x + 1) * (size_y + 1) = (5 * 5):
 *
 * 2) setup corner values using amplitude 3
 * {    -3.0        X          X          X          3.0   }
 * {     X          X          X          X          X     }
 * {     X          X          X          X          X     }
 * {     X          X          X          X          X     }
 * {     3.0        X          X          X         -3.0   }
 *
 * 3a) interpolate values in the middle
 * {    -3.0        X          0.0        X          3.0   }
 * {     X          X          X          X          X     }
 * {     0.0        X          0.0        X          0.0   }
 * {     X          X          X          X          X     }
 * {     3.0        X          0.0        X         -3.0   }
 *
 * 3b) add patches with amplitude 2 to them
 * {    -5.0        X          2.0        X          1.0   }
 * {     X          X          X          X          X     }
 * {     2.0        X         -2.0        X          2.0   }
 * {     X          X          X          X          X     }
 * {     1.0        X          2.0        X         -5.0   }
 *
 * 4a) interpolate values in the middle
 * {    -5.0       -1.5        2.0        1.5        1.0   }
 * {    -1.5       -0.75       0.0        0.75       1.5   }
 * {     2.0        0.0       -2.0        0.0        2.0   }
 * {     1.5        0.75       0.0       -0.75      -1.5   }
 * {     1.0        1.5        2.0       -1.5       -5.0   }
 *
 * 4b) add patches with amplitude 1 to them
 * {    -6.0       -0.5        1.0        2.5        0.0   }
 * {    -0.5       -1.75       1.0       -0.25       2.5   }
 * {     1.0        1.0       -3.0        1.0        1.0   }
 * {     2.5       -0.25       1.0       -1.75      -0.5   }
 * {     0.0        2.5        1.0       -0.5       -6.0   }
 *
 *
 *
 * Optimization 2:
 *
 * As you can see above, each noise function was called just once. Therefore
 * we don't need to use noise function that calculates the noise from x, y and
 * some prime. The same quality result we can obtain using standard Random()
 * function instead.
 *
 */

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#define M_PI   3.14159265358979323846
#endif /* M_PI_2 */

/** Fixed point type for heights */
typedef int16 height_t;
static const int height_decimal_bits = 4;
static const height_t _invalid_height = -32768;

/** Fixed point array for amplitudes (and percent values) */
typedef int amplitude_t;
static const int amplitude_decimal_bits = 10;

/** Height map - allocated array of heights (MapSizeX() + 1) x (MapSizeY() + 1) */
struct HeightMap
{
	height_t *h;         //< array of heights
	uint     dim_x;      //< height map size_x MapSizeX() + 1
	uint     total_size; //< height map total size
	uint     size_x;     //< MapSizeX()
	uint     size_y;     //< MapSizeY()
};

/** Global height map instance */
static HeightMap _height_map = {NULL, 0, 0, 0, 0};

/** Height map accessors */
#define HeightMapXY(x, y) _height_map.h[(x) + (y) * _height_map.dim_x]

/** Conversion: int to height_t */
#define I2H(i) ((i) << height_decimal_bits)
/** Conversion: height_t to int */
#define H2I(i) ((i) >> height_decimal_bits)

/** Conversion: int to amplitude_t */
#define I2A(i) ((i) << amplitude_decimal_bits)
/** Conversion: amplitude_t to int */
#define A2I(i) ((i) >> amplitude_decimal_bits)

/** Conversion: amplitude_t to height_t */
#define A2H(a) ((a) >> (amplitude_decimal_bits - height_decimal_bits))


/** Walk through all items of _height_map.h */
#define FOR_ALL_TILES_IN_HEIGHT(h) for (h = _height_map.h; h < &_height_map.h[_height_map.total_size]; h++)

/** Noise amplitudes (multiplied by 1024)
 * - indexed by "smoothness setting" and log2(frequency) */
static const amplitude_t _amplitudes_by_smoothness_and_frequency[4][12] = {
	/* Very smooth */
	{1000,  350,  123,   43,   15,    1,     1,    0,    0,    0,    0,    0},
	/* Smooth */
	{1000, 1000,  403,  200,   64,    8,     1,    0,    0,    0,    0,    0},
	/* Rough */
	{1000, 1200,  800,  500,  200,   16,     4,    0,    0,    0,    0,    0},
	/* Very Rough */
	{1500, 1000, 1200, 1000,  500,   32,    20,    0,    0,    0,    0,    0},
};

/** Desired water percentage (100% == 1024) - indexed by _settings.difficulty.quantity_sea_lakes */
static const amplitude_t _water_percent[4] = {20, 80, 250, 400};

/** Desired maximum height - indexed by _settings.difficulty.terrain_type */
static const int8 _max_height[4] = {
	6,       ///< Very flat
	9,       ///< Flat
	12,      ///< Hilly
	15       ///< Mountainous
};

/** Check if a X/Y set are within the map.
 * @param x coordinate x
 * @param y coordinate y
 * @return true if within the map
 */
static inline bool IsValidXY(uint x, uint y)
{
	return ((int)x) >= 0 && x < _height_map.size_x && ((int)y) >= 0 && y < _height_map.size_y;
}


/** Allocate array of (MapSizeX()+1)*(MapSizeY()+1) heights and init the _height_map structure members */
static inline bool AllocHeightMap()
{
	height_t *h;

	_height_map.size_x = MapSizeX();
	_height_map.size_y = MapSizeY();

	/* Allocate memory block for height map row pointers */
	_height_map.total_size = (_height_map.size_x + 1) * (_height_map.size_y + 1);
	_height_map.dim_x = _height_map.size_x + 1;
	_height_map.h = CallocT<height_t>(_height_map.total_size);
	if (_height_map.h == NULL) return false;

	/* Iterate through height map initialize values */
	FOR_ALL_TILES_IN_HEIGHT(h) *h = _invalid_height;

	return true;
}

/** Free height map */
static inline void FreeHeightMap()
{
	if (_height_map.h == NULL) return;
	free(_height_map.h);
	_height_map.h = NULL;
}

/** RandomHeight() generator */
static inline height_t RandomHeight(amplitude_t rMax)
{
	amplitude_t ra = (Random() << 16) | (Random() & 0x0000FFFF);
	height_t rh;
	/* Scale the amplitude for better resolution */
	rMax *= 16;
	/* Spread height into range -rMax..+rMax */
	rh = A2H(ra % (2 * rMax + 1) - rMax);
	return rh;
}

/** One interpolation and noise round */
static bool ApplyNoise(uint log_frequency, amplitude_t amplitude)
{
	uint size_min = min(_height_map.size_x, _height_map.size_y);
	uint step = size_min >> log_frequency;
	uint x, y;

	assert(_height_map.h != NULL);

	/* Are we finished? */
	if (step == 0) return false;

	if (log_frequency == 0) {
		/* This is first round, we need to establish base heights with step = size_min */
		for (y = 0; y <= _height_map.size_y; y += step) {
			for (x = 0; x <= _height_map.size_x; x += step) {
				height_t height = (amplitude > 0) ? RandomHeight(amplitude) : 0;
				HeightMapXY(x, y) = height;
			}
		}
		return true;
	}

	/* It is regular iteration round.
	 * Interpolate height values at odd x, even y tiles */
	for (y = 0; y <= _height_map.size_y; y += 2 * step) {
		for (x = 0; x < _height_map.size_x; x += 2 * step) {
			height_t h00 = HeightMapXY(x + 0 * step, y);
			height_t h02 = HeightMapXY(x + 2 * step, y);
			height_t h01 = (h00 + h02) / 2;
			HeightMapXY(x + 1 * step, y) = h01;
		}
	}

	/* Interpolate height values at odd y tiles */
	for (y = 0; y < _height_map.size_y; y += 2 * step) {
		for (x = 0; x <= _height_map.size_x; x += step) {
			height_t h00 = HeightMapXY(x, y + 0 * step);
			height_t h20 = HeightMapXY(x, y + 2 * step);
			height_t h10 = (h00 + h20) / 2;
			HeightMapXY(x, y + 1 * step) = h10;
		}
	}

	for (y = 0; y <= _height_map.size_y; y += step) {
		for (x = 0; x <= _height_map.size_x; x += step) {
			HeightMapXY(x, y) += RandomHeight(amplitude);
		}
	}
	return (step > 1);
}

/** Base Perlin noise generator - fills height map with raw Perlin noise */
static void HeightMapGenerate()
{
	uint size_min = min(_height_map.size_x, _height_map.size_y);
	uint iteration_round = 0;
	amplitude_t amplitude;
	bool continue_iteration;
	uint log_size_min, log_frequency_min;
	int log_frequency;

	/* Find first power of two that fits */
	for (log_size_min = 6; (1U << log_size_min) < size_min; log_size_min++) { }
	log_frequency_min = log_size_min - 6;

	do {
		log_frequency = iteration_round - log_frequency_min;
		if (log_frequency >= 0) {
			amplitude = _amplitudes_by_smoothness_and_frequency[_settings.game_creation.tgen_smoothness][log_frequency];
		} else {
			amplitude = 0;
		}
		continue_iteration = ApplyNoise(iteration_round, amplitude);
		iteration_round++;
	} while(continue_iteration);
}

/** Returns min, max and average height from height map */
static void HeightMapGetMinMaxAvg(height_t *min_ptr, height_t *max_ptr, height_t *avg_ptr)
{
	height_t h_min, h_max, h_avg, *h;
	int64 h_accu = 0;
	h_min = h_max = HeightMapXY(0, 0);

	/* Get h_min, h_max and accumulate heights into h_accu */
	FOR_ALL_TILES_IN_HEIGHT(h) {
		if (*h < h_min) h_min = *h;
		if (*h > h_max) h_max = *h;
		h_accu += *h;
	}

	/* Get average height */
	h_avg = (height_t)(h_accu / (_height_map.size_x * _height_map.size_y));

	/* Return required results */
	if (min_ptr != NULL) *min_ptr = h_min;
	if (max_ptr != NULL) *max_ptr = h_max;
	if (avg_ptr != NULL) *avg_ptr = h_avg;
}

/** Dill histogram and return pointer to its base point - to the count of zero heights */
static int *HeightMapMakeHistogram(height_t h_min, height_t h_max, int *hist_buf)
{
	int *hist = hist_buf - h_min;
	height_t *h;

	/* Fill histogram */
	FOR_ALL_TILES_IN_HEIGHT(h) {
		assert(*h >= h_min);
		assert(*h <= h_max);
		hist[*h]++;
	}
	return hist;
}

/** Applies sine wave redistribution onto height map */
static void HeightMapSineTransform(height_t h_min, height_t h_max)
{
	height_t *h;

	FOR_ALL_TILES_IN_HEIGHT(h) {
		double fheight;

		if (*h < h_min) continue;

		/* Transform height into 0..1 space */
		fheight = (double)(*h - h_min) / (double)(h_max - h_min);
		/* Apply sine transform depending on landscape type */
		switch(_settings.game_creation.landscape) {
			case LT_TOYLAND:
			case LT_TEMPERATE:
				/* Move and scale 0..1 into -1..+1 */
				fheight = 2 * fheight - 1;
				/* Sine transform */
				fheight = sin(fheight * M_PI_2);
				/* Transform it back from -1..1 into 0..1 space */
				fheight = 0.5 * (fheight + 1);
				break;

			case LT_ARCTIC:
				{
					/* Arctic terrain needs special height distribution.
					 * Redistribute heights to have more tiles at highest (75%..100%) range */
					double sine_upper_limit = 0.75;
					double linear_compression = 2;
					if (fheight >= sine_upper_limit) {
						/* Over the limit we do linear compression up */
						fheight = 1.0 - (1.0 - fheight) / linear_compression;
					} else {
						double m = 1.0 - (1.0 - sine_upper_limit) / linear_compression;
						/* Get 0..sine_upper_limit into -1..1 */
						fheight = 2.0 * fheight / sine_upper_limit - 1.0;
						/* Sine wave transform */
						fheight = sin(fheight * M_PI_2);
						/* Get -1..1 back to 0..(1 - (1 - sine_upper_limit) / linear_compression) == 0.0..m */
						fheight = 0.5 * (fheight + 1.0) * m;
					}
				}
				break;

			case LT_TROPIC:
				{
					/* Desert terrain needs special height distribution.
					 * Half of tiles should be at lowest (0..25%) heights */
					double sine_lower_limit = 0.5;
					double linear_compression = 2;
					if (fheight <= sine_lower_limit) {
						/* Under the limit we do linear compression down */
						fheight = fheight / linear_compression;
					} else {
						double m = sine_lower_limit / linear_compression;
						/* Get sine_lower_limit..1 into -1..1 */
						fheight = 2.0 * ((fheight - sine_lower_limit) / (1.0 - sine_lower_limit)) - 1.0;
						/* Sine wave transform */
						fheight = sin(fheight * M_PI_2);
						/* Get -1..1 back to (sine_lower_limit / linear_compression)..1.0 */
						fheight = 0.5 * ((1.0 - m) * fheight + (1.0 + m));
					}
				}
				break;

			default:
				NOT_REACHED();
				break;
		}
		/* Transform it back into h_min..h_max space */
		*h = (height_t)(fheight * (h_max - h_min) + h_min);
		if (*h < 0) *h = I2H(0);
		if (*h >= h_max) *h = h_max - 1;
	}
}

/** Adjusts heights in height map to contain required amount of water tiles */
static void HeightMapAdjustWaterLevel(amplitude_t water_percent, height_t h_max_new)
{
	height_t h_min, h_max, h_avg, h_water_level;
	int water_tiles, desired_water_tiles;
	height_t *h;
	int *hist;

	HeightMapGetMinMaxAvg(&h_min, &h_max, &h_avg);

	/* Allocate histogram buffer and clear its cells */
	int *hist_buf = CallocT<int>(h_max - h_min + 1);
	/* Fill histogram */
	hist = HeightMapMakeHistogram(h_min, h_max, hist_buf);

	/* How many water tiles do we want? */
	desired_water_tiles = (int)(((int64)water_percent) * (int64)(_height_map.size_x * _height_map.size_y)) >> amplitude_decimal_bits;

	/* Raise water_level and accumulate values from histogram until we reach required number of water tiles */
	for (h_water_level = h_min, water_tiles = 0; h_water_level < h_max; h_water_level++) {
		water_tiles += hist[h_water_level];
		if (water_tiles >= desired_water_tiles) break;
	}

	/* We now have the proper water level value.
	 * Transform the height map into new (normalized) height map:
	 *   values from range: h_min..h_water_level will become negative so it will be clamped to 0
	 *   values from range: h_water_level..h_max are transformed into 0..h_max_new
	 * , where h_max_new is 4, 8, 12 or 16 depending on terrain type (very flat, flat, hilly, mountains)
	 */
	FOR_ALL_TILES_IN_HEIGHT(h) {
		/* Transform height from range h_water_level..h_max into 0..h_max_new range */
		*h = (height_t)(((int)h_max_new) * (*h - h_water_level) / (h_max - h_water_level)) + I2H(1);
		/* Make sure all values are in the proper range (0..h_max_new) */
		if (*h < 0) *h = I2H(0);
		if (*h >= h_max_new) *h = h_max_new - 1;
	}

	free(hist_buf);
}

static double perlin_coast_noise_2D(const double x, const double y, const double p, const int prime);

/**
 * This routine sculpts in from the edge a random amount, again a Perlin
 * sequence, to avoid the rigid flat-edge slopes that were present before. The
 * Perlin noise map doesnt know where we are going to slice across, and so we
 * often cut straight through high terrain. the smoothing routine makes it
 * legal, gradually increasing up from the edge to the original terrain height.
 * By cutting parts of this away, it gives a far more irregular edge to the
 * map-edge. Sometimes it works beautifully with the existing sea & lakes, and
 * creates a very realistic coastline. Other times the variation is less, and
 * the map-edge shows its cliff-like roots.
 *
 * This routine may be extended to randomly sculpt the height of the terrain
 * near the edge. This will have the coast edge at low level (1-3), rising in
 * smoothed steps inland to about 15 tiles in. This should make it look as
 * though the map has been built for the map size, rather than a slice through
 * a larger map.
 *
 * Please note that all the small numbers; 53, 101, 167, etc. are small primes
 * to help give the perlin noise a bit more of a random feel.
 */
static void HeightMapCoastLines()
{
	int smallest_size = min(_settings.game_creation.map_x, _settings.game_creation.map_y);
	const int margin = 4;
	uint y, x;
	double max_x;
	double max_y;

	/* Lower to sea level */
	for (y = 0; y <= _height_map.size_y; y++) {
		/* Top right */
		max_x = abs((perlin_coast_noise_2D(_height_map.size_y - y, y, 0.9, 53) + 0.25) * 5 + (perlin_coast_noise_2D(y, y, 0.35, 179) + 1) * 12);
		max_x = max((smallest_size * smallest_size / 16) + max_x, (smallest_size * smallest_size / 16) + margin - max_x);
		if (smallest_size < 8 && max_x > 5) max_x /= 1.5;
		for (x = 0; x < max_x; x++) {
			HeightMapXY(x, y) = 0;
		}

		/* Bottom left */
		max_x = abs((perlin_coast_noise_2D(_height_map.size_y - y, y, 0.85, 101) + 0.3) * 6 + (perlin_coast_noise_2D(y, y, 0.45,  67) + 0.75) * 8);
		max_x = max((smallest_size * smallest_size / 16) + max_x, (smallest_size * smallest_size / 16) + margin - max_x);
		if (smallest_size < 8 && max_x > 5) max_x /= 1.5;
		for (x = _height_map.size_x; x > (_height_map.size_x - 1 - max_x); x--) {
			HeightMapXY(x, y) = 0;
		}
	}

	/* Lower to sea level */
	for (x = 0; x <= _height_map.size_x; x++) {
		/* Top left */
		max_y = abs((perlin_coast_noise_2D(x, _height_map.size_y / 2, 0.9, 167) + 0.4) * 5 + (perlin_coast_noise_2D(x, _height_map.size_y / 3, 0.4, 211) + 0.7) * 9);
		max_y = max((smallest_size * smallest_size / 16) + max_y, (smallest_size * smallest_size / 16) + margin - max_y);
		if (smallest_size < 8 && max_y > 5) max_y /= 1.5;
		for (y = 0; y < max_y; y++) {
			HeightMapXY(x, y) = 0;
		}


		/* Bottom right */
		max_y = abs((perlin_coast_noise_2D(x, _height_map.size_y / 3, 0.85, 71) + 0.25) * 6 + (perlin_coast_noise_2D(x, _height_map.size_y / 3, 0.35, 193) + 0.75) * 12);
		max_y = max((smallest_size * smallest_size / 16) + max_y, (smallest_size * smallest_size / 16) + margin - max_y);
		if (smallest_size < 8 && max_y > 5) max_y /= 1.5;
		for (y = _height_map.size_y; y > (_height_map.size_y - 1 - max_y); y--) {
			HeightMapXY(x, y) = 0;
		}
	}
}

/** Start at given point, move in given direction, find and Smooth coast in that direction */
static void HeightMapSmoothCoastInDirection(int org_x, int org_y, int dir_x, int dir_y)
{
	const int max_coast_dist_from_edge = 35;
	const int max_coast_Smooth_depth = 35;

	int x, y;
	int ed; // coast distance from edge
	int depth;

	height_t h_prev = 16;
	height_t h;

	assert(IsValidXY(org_x, org_y));

	/* Search for the coast (first non-water tile) */
	for (x = org_x, y = org_y, ed = 0; IsValidXY(x, y) && ed < max_coast_dist_from_edge; x += dir_x, y += dir_y, ed++) {
		/* Coast found? */
		if (HeightMapXY(x, y) > 15) break;

		/* Coast found in the neighborhood? */
		if (IsValidXY(x + dir_y, y + dir_x) && HeightMapXY(x + dir_y, y + dir_x) > 0) break;

		/* Coast found in the neighborhood on the other side */
		if (IsValidXY(x - dir_y, y - dir_x) && HeightMapXY(x - dir_y, y - dir_x) > 0) break;
	}

	/* Coast found or max_coast_dist_from_edge has been reached.
	 * Soften the coast slope */
	for (depth = 0; IsValidXY(x, y) && depth <= max_coast_Smooth_depth; depth++, x += dir_x, y += dir_y) {
		h = HeightMapXY(x, y);
		h = min(h, h_prev + (4 + depth)); // coast softening formula
		HeightMapXY(x, y) = h;
		h_prev = h;
	}
}

/** Smooth coasts by modulating height of tiles close to map edges with cosine of distance from edge */
static void HeightMapSmoothCoasts()
{
	uint x, y;
	/* First Smooth NW and SE coasts (y close to 0 and y close to size_y) */
	for (x = 0; x < _height_map.size_x; x++) {
		HeightMapSmoothCoastInDirection(x, 0, 0, 1);
		HeightMapSmoothCoastInDirection(x, _height_map.size_y - 1, 0, -1);
	}
	/* First Smooth NE and SW coasts (x close to 0 and x close to size_x) */
	for (y = 0; y < _height_map.size_y; y++) {
		HeightMapSmoothCoastInDirection(0, y, 1, 0);
		HeightMapSmoothCoastInDirection(_height_map.size_x - 1, y, -1, 0);
	}
}

/**
 * This routine provides the essential cleanup necessary before OTTD can
 * display the terrain. When generated, the terrain heights can jump more than
 * one level between tiles. This routine smooths out those differences so that
 * the most it can change is one level. When OTTD can support cliffs, this
 * routine may not be necessary.
 */
static void HeightMapSmoothSlopes(height_t dh_max)
{
	int x, y;
	for (y = 1; y <= (int)_height_map.size_y; y++) {
		for (x = 1; x <= (int)_height_map.size_x; x++) {
			height_t h_max = min(HeightMapXY(x - 1, y), HeightMapXY(x, y - 1)) + dh_max;
			if (HeightMapXY(x, y) > h_max) HeightMapXY(x, y) = h_max;
		}
	}
	for (y = _height_map.size_y - 1; y >= 0; y--) {
		for (x = _height_map.size_x - 1; x >= 0; x--) {
			height_t h_max = min(HeightMapXY(x + 1, y), HeightMapXY(x, y + 1)) + dh_max;
			if (HeightMapXY(x, y) > h_max) HeightMapXY(x, y) = h_max;
		}
	}
}

/** Height map terraform post processing:
 *  - water level adjusting
 *  - coast Smoothing
 *  - slope Smoothing
 *  - height histogram redistribution by sine wave transform */
static void HeightMapNormalize()
{
	const amplitude_t water_percent = _water_percent[_settings.difficulty.quantity_sea_lakes];
	const height_t h_max_new = I2H(_max_height[_settings.difficulty.terrain_type]);
	const height_t roughness = 7 + 3 * _settings.game_creation.tgen_smoothness;

	HeightMapAdjustWaterLevel(water_percent, h_max_new);

	HeightMapCoastLines();
	HeightMapSmoothSlopes(roughness);

	HeightMapSmoothCoasts();
	HeightMapSmoothSlopes(roughness);

	HeightMapSineTransform(12, h_max_new);
	HeightMapSmoothSlopes(16);
}

static inline int perlin_landXY(uint x, uint y)
{
	return HeightMapXY(x, y);
}


/**
 * The Perlin Noise calculation using large primes
 * The initial number is adjusted by two values; the generation_seed, and the
 * passed parameter; prime.
 * prime is used to allow the perlin noise generator to create useful random
 * numbers from slightly different series.
 */
static double int_noise(const long x, const long y, const int prime)
{
	long n = x + y * prime + _settings.game_creation.generation_seed;

	n = (n << 13) ^ n;

	/* Pseudo-random number generator, using several large primes */
	return 1.0 - (double)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0;
}


/**
 * Hj. Malthaner's routine included 2 different noise smoothing methods.
 * We now use the "raw" int_noise one.
 * However, it may be useful to move to the other routine in future.
 * So it is included too.
 */
static double smoothed_noise(const int x, const int y, const int prime)
{
#if 0
	/* A hilly world (four corner smooth) */
	const double sides = int_noise(x - 1, y) + int_noise(x + 1, y) + int_noise(x, y - 1) + int_noise(x, y + 1);
	const double center  =  int_noise(x, y);
	return (sides + sides + center * 4) / 8.0;
#endif

	/* This gives very hilly world */
	return int_noise(x, y, prime);
}


/**
 * This routine determines the interpolated value between a and b
 */
static inline double linear_interpolate(const double a, const double b, const double x)
{
	return a + x * (b - a);
}


/**
 * This routine returns the smoothed interpolated noise for an x and y, using
 * the values from the surrounding positions.
 */
static double interpolated_noise(const double x, const double y, const int prime)
{
	const int integer_X = (int)x;
	const int integer_Y = (int)y;

	const double fractional_X = x - (double)integer_X;
	const double fractional_Y = y - (double)integer_Y;

	const double v1 = smoothed_noise(integer_X,     integer_Y,     prime);
	const double v2 = smoothed_noise(integer_X + 1, integer_Y,     prime);
	const double v3 = smoothed_noise(integer_X,     integer_Y + 1, prime);
	const double v4 = smoothed_noise(integer_X + 1, integer_Y + 1, prime);

	const double i1 = linear_interpolate(v1, v2, fractional_X);
	const double i2 = linear_interpolate(v3, v4, fractional_X);

	return linear_interpolate(i1, i2, fractional_Y);
}


/**
 * This is a similar function to the main perlin noise calculation, but uses
 * the value p passed as a parameter rather than selected from the predefined
 * sequences. as you can guess by its title, i use this to create the indented
 * coastline, which is just another perlin sequence.
 */
static double perlin_coast_noise_2D(const double x, const double y, const double p, const int prime)
{
	double total = 0.0;
	int i;

	for (i = 0; i < 6; i++) {
		const double frequency = (double)(1 << i);
		const double amplitude = pow(p, (double)i);

		total += interpolated_noise((x * frequency) / 64.0, (y * frequency) / 64.0, prime) * amplitude;
	}

	return total;
}


/** A small helper function */
static void TgenSetTileHeight(TileIndex tile, int height)
{
	SetTileHeight(tile, height);
	MakeClear(tile, CLEAR_GRASS, 3);
}

/**
 * The main new land generator using Perlin noise. Desert landscape is handled
 * different to all others to give a desert valley between two high mountains.
 * Clearly if a low height terrain (flat/very flat) is chosen, then the tropic
 * areas wont be high enough, and there will be very little tropic on the map.
 * Thus Tropic works best on Hilly or Mountainous.
 */
void GenerateTerrainPerlin()
{
	uint x, y;

	if (!AllocHeightMap()) return;
	GenerateWorldSetAbortCallback(FreeHeightMap);

	HeightMapGenerate();

	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	HeightMapNormalize();

	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	/* Transfer height map into OTTD map */
	for (y = 2; y < _height_map.size_y - 2; y++) {
		for (x = 2; x < _height_map.size_x - 2; x++) {
			int height = H2I(HeightMapXY(x, y));
			if (height < 0) height = 0;
			if (height > 15) height = 15;
			TgenSetTileHeight(TileXY(x, y), height);
		}
	}

	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	/* Recreate void tiles at the border in case they have been affected by generation */
	for (y = 0; y < _height_map.size_y - 1; y++) MakeVoid(_height_map.size_x * y + _height_map.size_x - 1);
	for (x = 0; x < _height_map.size_x;     x++) MakeVoid(_height_map.size_x * y + x);

	FreeHeightMap();
	GenerateWorldSetAbortCallback(NULL);
}
