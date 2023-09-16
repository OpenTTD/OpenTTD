/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tgp.cpp OTTD Perlin Noise Landscape Generator, aka TerraGenesis Perlin */

#include "stdafx.h"
#include "clear_map.h"
#include "void_map.h"
#include "genworld.h"
#include "core/random_func.hpp"
#include "landscape_type.h"

#include "safeguards.h"

/*
 *
 * Quickie guide to Perlin Noise
 * Perlin noise is a predictable pseudo random number sequence. By generating
 * it in 2 dimensions, it becomes a useful random map that, for a given seed
 * and starting X & Y, is entirely predictable. On the face of it, that may not
 * be useful. However, it means that if you want to replay a map in a different
 * terrain, or just vary the sea level, you just re-run the generator with the
 * same seed. The seed is an int32_t, and is randomised on each run of New Game.
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
 * Usefully, this routine is totally scalable; so when 32bpp comes along, the
 * terrain can be as bumpy as you like! It is also infinitely expandable; a
 * single random seed terrain continues in X & Y as far as you care to
 * calculate. In theory, we could use just one seed value, but randomly select
 * where in the Perlin XY space we use for the terrain. Personally I prefer
 * using a simple (0, 0) to (X, Y), with a varying seed.
 *
 *
 * Other things i have had to do: mountainous wasn't mountainous enough, and
 * since we only have 0..15 heights available, I add a second generated map
 * (with a modified seed), onto the original. This generally raises the
 * terrain, which then needs scaling back down. Overall effect is a general
 * uplift.
 *
 * However, the values on the top of mountains are then almost guaranteed to go
 * too high, so large flat plateaus appeared at height 15. To counter this, I
 * scale all heights above 12 to proportion up to 15. It still makes the
 * mountains have flattish tops, rather than craggy peaks, but at least they
 * aren't smooth as glass.
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
 * The overall speedup should be 300% to 800% depending on CPU and memory speed.
 *
 * I will try to explain it on the example below:
 *
 * Have a map of 4 x 4 tiles, our simplified noise generator produces only two
 * values -1 and +1, use 3 octaves with wave length 1, 2 and 4, with amplitudes
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

/** Fixed point type for heights */
using Height = int16_t;
static const int height_decimal_bits = 4;

/** Fixed point array for amplitudes (and percent values) */
using Amplitude = int;
static const int amplitude_decimal_bits = 10;

/** Height map - allocated array of heights (MapSizeX() + 1) x (MapSizeY() + 1) */
struct HeightMap
{
	std::vector<Height> h; //< array of heights
	/* Even though the sizes are always positive, there are many cases where
	 * X and Y need to be signed integers due to subtractions. */
	int      dim_x;      //< height map size_x Map::SizeX() + 1
	int      size_x;     //< Map::SizeX()
	int      size_y;     //< Map::SizeY()

	/**
	 * Height map accessor
	 * @param x X position
	 * @param y Y position
	 * @return height as fixed point number
	 */
	inline Height &height(uint x, uint y)
	{
		return h[x + y * dim_x];
	}
};

/** Global height map instance */
static HeightMap _height_map = { {}, 0, 0, 0 };

/** Conversion: int to Height */
#define I2H(i) ((i) << height_decimal_bits)
/** Conversion: Height to int */
#define H2I(i) ((i) >> height_decimal_bits)

/** Conversion: int to Amplitude */
#define I2A(i) ((i) << amplitude_decimal_bits)
/** Conversion: Amplitude to int */
#define A2I(i) ((i) >> amplitude_decimal_bits)

/** Conversion: Amplitude to Height */
#define A2H(a) ((a) >> (amplitude_decimal_bits - height_decimal_bits))

/** Maximum number of TGP noise frequencies. */
static const int MAX_TGP_FREQUENCIES = 10;

/** Desired water percentage (100% == 1024) - indexed by _settings_game.difficulty.quantity_sea_lakes */
static const Amplitude _water_percent[4] = {70, 170, 270, 420};

/**
 * Gets the maximum allowed height while generating a map based on
 * mapsize, terraintype, and the maximum height level.
 * @return The maximum height for the map generation.
 * @note Values should never be lower than 3 since the minimum snowline height is 2.
 */
static Height TGPGetMaxHeight()
{
	if (_settings_game.difficulty.terrain_type == CUSTOM_TERRAIN_TYPE_NUMBER_DIFFICULTY) {
		/* TGP never reaches this height; this means that if a user inputs "2",
		 * it would create a flat map without the "+ 1". But that would
		 * overflow on "255". So we reduce it by 1 to get back in range. */
		return I2H(_settings_game.game_creation.custom_terrain_type + 1) - 1;
	}

	/**
	 * Desired maximum height - indexed by:
	 *  - _settings_game.difficulty.terrain_type
	 *  - min(Map::LogX(), Map::LogY()) - MIN_MAP_SIZE_BITS
	 *
	 * It is indexed by map size as well as terrain type since the map size limits the height of
	 * a usable mountain. For example, on a 64x64 map a 24 high single peak mountain (as if you
	 * raised land 24 times in the center of the map) will leave only a ring of about 10 tiles
	 * around the mountain to build on. On a 4096x4096 map, it won't cover any major part of the map.
	 */
	static const int max_height[5][MAX_MAP_SIZE_BITS - MIN_MAP_SIZE_BITS + 1] = {
		/* 64  128  256  512 1024 2048 4096 */
		{   3,   3,   3,   3,   4,   5,   7 }, ///< Very flat
		{   5,   7,   8,   9,  14,  19,  31 }, ///< Flat
		{   8,   9,  10,  15,  23,  37,  61 }, ///< Hilly
		{  10,  11,  17,  19,  49,  63,  73 }, ///< Mountainous
		{  12,  19,  25,  31,  67,  75,  87 }, ///< Alpinist
	};

	int map_size_bucket = std::min(Map::LogX(), Map::LogY()) - MIN_MAP_SIZE_BITS;
	int max_height_from_table = max_height[_settings_game.difficulty.terrain_type][map_size_bucket];

	/* If there is a manual map height limit, clamp to it. */
	if (_settings_game.construction.map_height_limit != 0) {
		max_height_from_table = std::min<uint>(max_height_from_table, _settings_game.construction.map_height_limit);
	}

	return I2H(max_height_from_table);
}

/**
 * Get an overestimation of the highest peak TGP wants to generate.
 */
uint GetEstimationTGPMapHeight()
{
	return H2I(TGPGetMaxHeight());
}

/**
 * Get the amplitude associated with the currently selected
 * smoothness and maximum height level.
 * @param frequency The frequency to get the amplitudes for
 * @return The amplitudes to apply to the map.
 */
static Amplitude GetAmplitude(int frequency)
{
	/* Base noise amplitudes (multiplied by 1024) and indexed by "smoothness setting" and log2(frequency). */
	static const Amplitude amplitudes[][7] = {
		/* lowest frequency ...... highest (every corner) */
		{16000,  5600,  1968,   688,   240,    16,    16}, ///< Very smooth
		{24000, 12800,  6400,  2700,  1024,   128,    16}, ///< Smooth
		{32000, 19200, 12800,  8000,  3200,   256,    64}, ///< Rough
		{48000, 24000, 19200, 16000,  8000,   512,   320}, ///< Very rough
	};
	/*
	 * Extrapolation factors for ranges before the table.
	 * The extrapolation is needed to account for the higher map heights. They need larger
	 * areas with a particular gradient so that we are able to create maps without too
	 * many steep slopes up to the wanted height level. It's definitely not perfect since
	 * it will bring larger rectangles with similar slopes which makes the rectangular
	 * behaviour of TGP more noticeable. However, these height differentiations cannot
	 * happen over much smaller areas; we basically double the "range" to give a similar
	 * slope for every doubling of map height.
	 */
	static const double extrapolation_factors[] = { 3.3, 2.8, 2.3, 1.8 };

	int smoothness = _settings_game.game_creation.tgen_smoothness;

	/* Get the table index, and return that value if possible. */
	int index = frequency - MAX_TGP_FREQUENCIES + lengthof(amplitudes[smoothness]);
	Amplitude amplitude = amplitudes[smoothness][std::max(0, index)];
	if (index >= 0) return amplitude;

	/* We need to extrapolate the amplitude. */
	double extrapolation_factor = extrapolation_factors[smoothness];
	int height_range = I2H(16);
	do {
		amplitude = (Amplitude)(extrapolation_factor * (double)amplitude);
		height_range <<= 1;
		index++;
	} while (index < 0);

	return Clamp((TGPGetMaxHeight() - height_range) / height_range, 0, 1) * amplitude;
}

/**
 * Check if a X/Y set are within the map.
 * @param x coordinate x
 * @param y coordinate y
 * @return true if within the map
 */
static inline bool IsValidXY(int x, int y)
{
	return x >= 0 && x < _height_map.size_x && y >= 0 && y < _height_map.size_y;
}


/**
 * Allocate array of (MapSizeX()+1)*(MapSizeY()+1) heights and init the _height_map structure members
 * @return true on success
 */
static inline bool AllocHeightMap()
{
	assert(_height_map.h.empty());

	_height_map.size_x = Map::SizeX();
	_height_map.size_y = Map::SizeY();

	/* Allocate memory block for height map row pointers */
	size_t total_size = static_cast<size_t>(_height_map.size_x + 1) * (_height_map.size_y + 1);
	_height_map.dim_x = _height_map.size_x + 1;
	_height_map.h.resize(total_size);

	return true;
}

/** Free height map */
static inline void FreeHeightMap()
{
	_height_map.h.clear();
}

/**
 * Generates new random height in given amplitude (generated numbers will range from - amplitude to + amplitude)
 * @param rMax Limit of result
 * @return generated height
 */
static inline Height RandomHeight(Amplitude rMax)
{
	/* Spread height into range -rMax..+rMax */
	return A2H(RandomRange(2 * rMax + 1) - rMax);
}

/**
 * Base Perlin noise generator - fills height map with raw Perlin noise.
 *
 * This runs several iterations with increasing precision; the last iteration looks at areas
 * of 1 by 1 tiles, the second to last at 2 by 2 tiles and the initial 2**MAX_TGP_FREQUENCIES
 * by 2**MAX_TGP_FREQUENCIES tiles.
 */
static void HeightMapGenerate()
{
	/* Trying to apply noise to uninitialized height map */
	assert(!_height_map.h.empty());

	int start = std::max(MAX_TGP_FREQUENCIES - (int)std::min(Map::LogX(), Map::LogY()), 0);
	bool first = true;

	for (int frequency = start; frequency < MAX_TGP_FREQUENCIES; frequency++) {
		const Amplitude amplitude = GetAmplitude(frequency);

		/* Ignore zero amplitudes; it means our map isn't height enough for this
		 * amplitude, so ignore it and continue with the next set of amplitude. */
		if (amplitude == 0) continue;

		const int step = 1 << (MAX_TGP_FREQUENCIES - frequency - 1);

		if (first) {
			/* This is first round, we need to establish base heights with step = size_min */
			for (int y = 0; y <= _height_map.size_y; y += step) {
				for (int x = 0; x <= _height_map.size_x; x += step) {
					Height height = (amplitude > 0) ? RandomHeight(amplitude) : 0;
					_height_map.height(x, y) = height;
				}
			}
			first = false;
			continue;
		}

		/* It is regular iteration round.
		 * Interpolate height values at odd x, even y tiles */
		for (int y = 0; y <= _height_map.size_y; y += 2 * step) {
			for (int x = 0; x <= _height_map.size_x - 2 * step; x += 2 * step) {
				Height h00 = _height_map.height(x + 0 * step, y);
				Height h02 = _height_map.height(x + 2 * step, y);
				Height h01 = (h00 + h02) / 2;
				_height_map.height(x + 1 * step, y) = h01;
			}
		}

		/* Interpolate height values at odd y tiles */
		for (int y = 0; y <= _height_map.size_y - 2 * step; y += 2 * step) {
			for (int x = 0; x <= _height_map.size_x; x += step) {
				Height h00 = _height_map.height(x, y + 0 * step);
				Height h20 = _height_map.height(x, y + 2 * step);
				Height h10 = (h00 + h20) / 2;
				_height_map.height(x, y + 1 * step) = h10;
			}
		}

		/* Add noise for next higher frequency (smaller steps) */
		for (int y = 0; y <= _height_map.size_y; y += step) {
			for (int x = 0; x <= _height_map.size_x; x += step) {
				_height_map.height(x, y) += RandomHeight(amplitude);
			}
		}
	}
}

/** Returns min, max and average height from height map */
static void HeightMapGetMinMaxAvg(Height *min_ptr, Height *max_ptr, Height *avg_ptr)
{
	Height h_min, h_max, h_avg;
	int64_t h_accu = 0;
	h_min = h_max = _height_map.height(0, 0);

	/* Get h_min, h_max and accumulate heights into h_accu */
	for (const Height &h : _height_map.h) {
		if (h < h_min) h_min = h;
		if (h > h_max) h_max = h;
		h_accu += h;
	}

	/* Get average height */
	h_avg = (Height)(h_accu / (_height_map.size_x * _height_map.size_y));

	/* Return required results */
	if (min_ptr != nullptr) *min_ptr = h_min;
	if (max_ptr != nullptr) *max_ptr = h_max;
	if (avg_ptr != nullptr) *avg_ptr = h_avg;
}

/** Dill histogram and return pointer to its base point - to the count of zero heights */
static int *HeightMapMakeHistogram(Height h_min, [[maybe_unused]] Height h_max, int *hist_buf)
{
	int *hist = hist_buf - h_min;

	/* Count the heights and fill the histogram */
	for (const Height &h : _height_map.h){
		assert(h >= h_min);
		assert(h <= h_max);
		hist[h]++;
	}
	return hist;
}

/** Applies sine wave redistribution onto height map */
static void HeightMapSineTransform(Height h_min, Height h_max)
{
	for (Height &h : _height_map.h) {
		double fheight;

		if (h < h_min) continue;

		/* Transform height into 0..1 space */
		fheight = (double)(h - h_min) / (double)(h_max - h_min);
		/* Apply sine transform depending on landscape type */
		switch (_settings_game.game_creation.landscape) {
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
		h = (Height)(fheight * (h_max - h_min) + h_min);
		if (h < 0) h = I2H(0);
		if (h >= h_max) h = h_max - 1;
	}
}

/**
 * Additional map variety is provided by applying different curve maps
 * to different parts of the map. A randomized low resolution grid contains
 * which curve map to use on each part of the make. This filtered non-linearly
 * to smooth out transitions between curves, so each tile could have between
 * 100% of one map applied or 25% of four maps.
 *
 * The curve maps define different land styles, i.e. lakes, low-lands, hills
 * and mountain ranges, although these are dependent on the landscape style
 * chosen as well.
 *
 * The level parameter dictates the resolution of the grid. A low resolution
 * grid will result in larger continuous areas of a land style, a higher
 * resolution grid splits the style into smaller areas.
 * @param level Rough indication of the size of the grid sections to style. Small level means large grid sections.
 */
static void HeightMapCurves(uint level)
{
	Height mh = TGPGetMaxHeight() - I2H(1); // height levels above sea level only

	/** Basically scale height X to height Y. Everything in between is interpolated. */
	struct ControlPoint {
		Height x; ///< The height to scale from.
		Height y; ///< The height to scale to.
	};
	/* Scaled curve maps; value is in height_ts. */
#define F(fraction) ((Height)(fraction * mh))
	const ControlPoint curve_map_1[] = { { F(0.0), F(0.0) },                       { F(0.8), F(0.13) },                       { F(1.0), F(0.4)  } };
	const ControlPoint curve_map_2[] = { { F(0.0), F(0.0) }, { F(0.53), F(0.13) }, { F(0.8), F(0.27) },                       { F(1.0), F(0.6)  } };
	const ControlPoint curve_map_3[] = { { F(0.0), F(0.0) }, { F(0.53), F(0.27) }, { F(0.8), F(0.57) },                       { F(1.0), F(0.8)  } };
	const ControlPoint curve_map_4[] = { { F(0.0), F(0.0) }, { F(0.4),  F(0.3)  }, { F(0.7), F(0.8)  }, { F(0.92), F(0.99) }, { F(1.0), F(0.99) } };
#undef F

	/** Helper structure to index the different curve maps. */
	struct ControlPointList {
		size_t length;            ///< The length of the curve map.
		const ControlPoint *list; ///< The actual curve map.
	};
	static const ControlPointList curve_maps[] = {
		{ lengthof(curve_map_1), curve_map_1 },
		{ lengthof(curve_map_2), curve_map_2 },
		{ lengthof(curve_map_3), curve_map_3 },
		{ lengthof(curve_map_4), curve_map_4 },
	};

	Height ht[lengthof(curve_maps)];
	MemSetT(ht, 0, lengthof(ht));

	/* Set up a grid to choose curve maps based on location; attempt to get a somewhat square grid */
	float factor = sqrt((float)_height_map.size_x / (float)_height_map.size_y);
	uint sx = Clamp((int)(((1 << level) * factor) + 0.5), 1, 128);
	uint sy = Clamp((int)(((1 << level) / factor) + 0.5), 1, 128);
	std::vector<byte> c(static_cast<size_t>(sx) * sy);

	for (uint i = 0; i < sx * sy; i++) {
		c[i] = Random() % lengthof(curve_maps);
	}

	/* Apply curves */
	for (int x = 0; x < _height_map.size_x; x++) {

		/* Get our X grid positions and bi-linear ratio */
		float fx = (float)(sx * x) / _height_map.size_x + 1.0f;
		uint x1 = (uint)fx;
		uint x2 = x1;
		float xr = 2.0f * (fx - x1) - 1.0f;
		xr = sin(xr * M_PI_2);
		xr = sin(xr * M_PI_2);
		xr = 0.5f * (xr + 1.0f);
		float xri = 1.0f - xr;

		if (x1 > 0) {
			x1--;
			if (x2 >= sx) x2--;
		}

		for (int y = 0; y < _height_map.size_y; y++) {

			/* Get our Y grid position and bi-linear ratio */
			float fy = (float)(sy * y) / _height_map.size_y + 1.0f;
			uint y1 = (uint)fy;
			uint y2 = y1;
			float yr = 2.0f * (fy - y1) - 1.0f;
			yr = sin(yr * M_PI_2);
			yr = sin(yr * M_PI_2);
			yr = 0.5f * (yr + 1.0f);
			float yri = 1.0f - yr;

			if (y1 > 0) {
				y1--;
				if (y2 >= sy) y2--;
			}

			uint corner_a = c[x1 + sx * y1];
			uint corner_b = c[x1 + sx * y2];
			uint corner_c = c[x2 + sx * y1];
			uint corner_d = c[x2 + sx * y2];

			/* Bitmask of which curve maps are chosen, so that we do not bother
			 * calculating a curve which won't be used. */
			uint corner_bits = 0;
			corner_bits |= 1 << corner_a;
			corner_bits |= 1 << corner_b;
			corner_bits |= 1 << corner_c;
			corner_bits |= 1 << corner_d;

			Height *h = &_height_map.height(x, y);

			/* Do not touch sea level */
			if (*h < I2H(1)) continue;

			/* Only scale above sea level */
			*h -= I2H(1);

			/* Apply all curve maps that are used on this tile. */
			for (uint t = 0; t < lengthof(curve_maps); t++) {
				if (!HasBit(corner_bits, t)) continue;

				[[maybe_unused]] bool found = false;
				const ControlPoint *cm = curve_maps[t].list;
				for (uint i = 0; i < curve_maps[t].length - 1; i++) {
					const ControlPoint &p1 = cm[i];
					const ControlPoint &p2 = cm[i + 1];

					if (*h >= p1.x && *h < p2.x) {
						ht[t] = p1.y + (*h - p1.x) * (p2.y - p1.y) / (p2.x - p1.x);
#ifdef WITH_ASSERT
						found = true;
#endif
						break;
					}
				}
				assert(found);
			}

			/* Apply interpolation of curve map results. */
			*h = (Height)((ht[corner_a] * yri + ht[corner_b] * yr) * xri + (ht[corner_c] * yri + ht[corner_d] * yr) * xr);

			/* Readd sea level */
			*h += I2H(1);
		}
	}
}

/** Adjusts heights in height map to contain required amount of water tiles */
static void HeightMapAdjustWaterLevel(Amplitude water_percent, Height h_max_new)
{
	Height h_min, h_max, h_avg, h_water_level;
	int64_t water_tiles, desired_water_tiles;
	int *hist;

	HeightMapGetMinMaxAvg(&h_min, &h_max, &h_avg);

	/* Allocate histogram buffer and clear its cells */
	int *hist_buf = CallocT<int>(h_max - h_min + 1);
	/* Fill histogram */
	hist = HeightMapMakeHistogram(h_min, h_max, hist_buf);

	/* How many water tiles do we want? */
	desired_water_tiles = A2I(((int64_t)water_percent) * (int64_t)(_height_map.size_x * _height_map.size_y));

	/* Raise water_level and accumulate values from histogram until we reach required number of water tiles */
	for (h_water_level = h_min, water_tiles = 0; h_water_level < h_max; h_water_level++) {
		water_tiles += hist[h_water_level];
		if (water_tiles >= desired_water_tiles) break;
	}

	/* We now have the proper water level value.
	 * Transform the height map into new (normalized) height map:
	 *   values from range: h_min..h_water_level will become negative so it will be clamped to 0
	 *   values from range: h_water_level..h_max are transformed into 0..h_max_new
	 *   where h_max_new is depending on terrain type and map size.
	 */
	for (Height &h : _height_map.h) {
		/* Transform height from range h_water_level..h_max into 0..h_max_new range */
		h = (Height)(((int)h_max_new) * (h - h_water_level) / (h_max - h_water_level)) + I2H(1);
		/* Make sure all values are in the proper range (0..h_max_new) */
		if (h < 0) h = I2H(0);
		if (h >= h_max_new) h = h_max_new - 1;
	}

	free(hist_buf);
}

static double perlin_coast_noise_2D(const double x, const double y, const double p, const int prime);

/**
 * This routine sculpts in from the edge a random amount, again a Perlin
 * sequence, to avoid the rigid flat-edge slopes that were present before. The
 * Perlin noise map doesn't know where we are going to slice across, and so we
 * often cut straight through high terrain. The smoothing routine makes it
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
static void HeightMapCoastLines(uint8_t water_borders)
{
	int smallest_size = std::min(_settings_game.game_creation.map_x, _settings_game.game_creation.map_y);
	const int margin = 4;
	int y, x;
	double max_x;
	double max_y;

	/* Lower to sea level */
	for (y = 0; y <= _height_map.size_y; y++) {
		if (HasBit(water_borders, BORDER_NE)) {
			/* Top right */
			max_x = abs((perlin_coast_noise_2D(_height_map.size_y - y, y, 0.9, 53) + 0.25) * 5 + (perlin_coast_noise_2D(y, y, 0.35, 179) + 1) * 12);
			max_x = std::max((smallest_size * smallest_size / 64) + max_x, (smallest_size * smallest_size / 64) + margin - max_x);
			if (smallest_size < 8 && max_x > 5) max_x /= 1.5;
			for (x = 0; x < max_x; x++) {
				_height_map.height(x, y) = 0;
			}
		}

		if (HasBit(water_borders, BORDER_SW)) {
			/* Bottom left */
			max_x = abs((perlin_coast_noise_2D(_height_map.size_y - y, y, 0.85, 101) + 0.3) * 6 + (perlin_coast_noise_2D(y, y, 0.45,  67) + 0.75) * 8);
			max_x = std::max((smallest_size * smallest_size / 64) + max_x, (smallest_size * smallest_size / 64) + margin - max_x);
			if (smallest_size < 8 && max_x > 5) max_x /= 1.5;
			for (x = _height_map.size_x; x > (_height_map.size_x - 1 - max_x); x--) {
				_height_map.height(x, y) = 0;
			}
		}
	}

	/* Lower to sea level */
	for (x = 0; x <= _height_map.size_x; x++) {
		if (HasBit(water_borders, BORDER_NW)) {
			/* Top left */
			max_y = abs((perlin_coast_noise_2D(x, _height_map.size_y / 2, 0.9, 167) + 0.4) * 5 + (perlin_coast_noise_2D(x, _height_map.size_y / 3, 0.4, 211) + 0.7) * 9);
			max_y = std::max((smallest_size * smallest_size / 64) + max_y, (smallest_size * smallest_size / 64) + margin - max_y);
			if (smallest_size < 8 && max_y > 5) max_y /= 1.5;
			for (y = 0; y < max_y; y++) {
				_height_map.height(x, y) = 0;
			}
		}

		if (HasBit(water_borders, BORDER_SE)) {
			/* Bottom right */
			max_y = abs((perlin_coast_noise_2D(x, _height_map.size_y / 3, 0.85, 71) + 0.25) * 6 + (perlin_coast_noise_2D(x, _height_map.size_y / 3, 0.35, 193) + 0.75) * 12);
			max_y = std::max((smallest_size * smallest_size / 64) + max_y, (smallest_size * smallest_size / 64) + margin - max_y);
			if (smallest_size < 8 && max_y > 5) max_y /= 1.5;
			for (y = _height_map.size_y; y > (_height_map.size_y - 1 - max_y); y--) {
				_height_map.height(x, y) = 0;
			}
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

	Height h_prev = I2H(1);
	Height h;

	assert(IsValidXY(org_x, org_y));

	/* Search for the coast (first non-water tile) */
	for (x = org_x, y = org_y, ed = 0; IsValidXY(x, y) && ed < max_coast_dist_from_edge; x += dir_x, y += dir_y, ed++) {
		/* Coast found? */
		if (_height_map.height(x, y) >= I2H(1)) break;

		/* Coast found in the neighborhood? */
		if (IsValidXY(x + dir_y, y + dir_x) && _height_map.height(x + dir_y, y + dir_x) > 0) break;

		/* Coast found in the neighborhood on the other side */
		if (IsValidXY(x - dir_y, y - dir_x) && _height_map.height(x - dir_y, y - dir_x) > 0) break;
	}

	/* Coast found or max_coast_dist_from_edge has been reached.
	 * Soften the coast slope */
	for (depth = 0; IsValidXY(x, y) && depth <= max_coast_Smooth_depth; depth++, x += dir_x, y += dir_y) {
		h = _height_map.height(x, y);
		h = static_cast<Height>(std::min<uint>(h, h_prev + (4 + depth))); // coast softening formula
		_height_map.height(x, y) = h;
		h_prev = h;
	}
}

/** Smooth coasts by modulating height of tiles close to map edges with cosine of distance from edge */
static void HeightMapSmoothCoasts(uint8_t water_borders)
{
	int x, y;
	/* First Smooth NW and SE coasts (y close to 0 and y close to size_y) */
	for (x = 0; x < _height_map.size_x; x++) {
		if (HasBit(water_borders, BORDER_NW)) HeightMapSmoothCoastInDirection(x, 0, 0, 1);
		if (HasBit(water_borders, BORDER_SE)) HeightMapSmoothCoastInDirection(x, _height_map.size_y - 1, 0, -1);
	}
	/* First Smooth NE and SW coasts (x close to 0 and x close to size_x) */
	for (y = 0; y < _height_map.size_y; y++) {
		if (HasBit(water_borders, BORDER_NE)) HeightMapSmoothCoastInDirection(0, y, 1, 0);
		if (HasBit(water_borders, BORDER_SW)) HeightMapSmoothCoastInDirection(_height_map.size_x - 1, y, -1, 0);
	}
}

/**
 * This routine provides the essential cleanup necessary before OTTD can
 * display the terrain. When generated, the terrain heights can jump more than
 * one level between tiles. This routine smooths out those differences so that
 * the most it can change is one level. When OTTD can support cliffs, this
 * routine may not be necessary.
 */
static void HeightMapSmoothSlopes(Height dh_max)
{
	for (int y = 0; y <= (int)_height_map.size_y; y++) {
		for (int x = 0; x <= (int)_height_map.size_x; x++) {
			Height h_max = std::min(_height_map.height(x > 0 ? x - 1 : x, y), _height_map.height(x, y > 0 ? y - 1 : y)) + dh_max;
			if (_height_map.height(x, y) > h_max) _height_map.height(x, y) = h_max;
		}
	}
	for (int y = _height_map.size_y; y >= 0; y--) {
		for (int x = _height_map.size_x; x >= 0; x--) {
			Height h_max = std::min(_height_map.height(x < _height_map.size_x ? x + 1 : x, y), _height_map.height(x, y < _height_map.size_y ? y + 1 : y)) + dh_max;
			if (_height_map.height(x, y) > h_max) _height_map.height(x, y) = h_max;
		}
	}
}

/**
 * Height map terraform post processing:
 *  - water level adjusting
 *  - coast Smoothing
 *  - slope Smoothing
 *  - height histogram redistribution by sine wave transform
 */
static void HeightMapNormalize()
{
	int sea_level_setting = _settings_game.difficulty.quantity_sea_lakes;
	const Amplitude water_percent = sea_level_setting != (int)CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY ? _water_percent[sea_level_setting] : _settings_game.game_creation.custom_sea_level * 1024 / 100;
	const Height h_max_new = TGPGetMaxHeight();
	const Height roughness = 7 + 3 * _settings_game.game_creation.tgen_smoothness;

	HeightMapAdjustWaterLevel(water_percent, h_max_new);

	byte water_borders = _settings_game.construction.freeform_edges ? _settings_game.game_creation.water_borders : 0xF;
	if (water_borders == BORDERS_RANDOM) water_borders = GB(Random(), 0, 4);

	HeightMapCoastLines(water_borders);
	HeightMapSmoothSlopes(roughness);

	HeightMapSmoothCoasts(water_borders);
	HeightMapSmoothSlopes(roughness);

	HeightMapSineTransform(I2H(1), h_max_new);

	if (_settings_game.game_creation.variety > 0) {
		HeightMapCurves(_settings_game.game_creation.variety);
	}

	HeightMapSmoothSlopes(I2H(1));
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
	long n = x + y * prime + _settings_game.game_creation.generation_seed;

	n = (n << 13) ^ n;

	/* Pseudo-random number generator, using several large primes */
	return 1.0 - (double)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0;
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

	const double v1 = int_noise(integer_X,     integer_Y,     prime);
	const double v2 = int_noise(integer_X + 1, integer_Y,     prime);
	const double v3 = int_noise(integer_X,     integer_Y + 1, prime);
	const double v4 = int_noise(integer_X + 1, integer_Y + 1, prime);

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

	for (int i = 0; i < 6; i++) {
		const double frequency = (double)(1 << i);
		const double amplitude = pow(p, (double)i);

		total += interpolated_noise((x * frequency) / 64.0, (y * frequency) / 64.0, prime) * amplitude;
	}

	return total;
}


/** A small helper function to initialize the terrain */
static void TgenSetTileHeight(TileIndex tile, int height)
{
	SetTileHeight(tile, height);

	/* Only clear the tiles within the map area. */
	if (IsInnerTile(tile)) {
		MakeClear(tile, CLEAR_GRASS, 3);
	}
}

/**
 * The main new land generator using Perlin noise. Desert landscape is handled
 * different to all others to give a desert valley between two high mountains.
 * Clearly if a low height terrain (flat/very flat) is chosen, then the tropic
 * areas won't be high enough, and there will be very little tropic on the map.
 * Thus Tropic works best on Hilly or Mountainous.
 */
void GenerateTerrainPerlin()
{
	if (!AllocHeightMap()) return;
	GenerateWorldSetAbortCallback(FreeHeightMap);

	HeightMapGenerate();

	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	HeightMapNormalize();

	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	/* First make sure the tiles at the north border are void tiles if needed. */
	if (_settings_game.construction.freeform_edges) {
		for (uint x = 0; x < Map::SizeX(); x++) MakeVoid(TileXY(x, 0));
		for (uint y = 0; y < Map::SizeY(); y++) MakeVoid(TileXY(0, y));
	}

	int max_height = H2I(TGPGetMaxHeight());

	/* Transfer height map into OTTD map */
	for (int y = 0; y < _height_map.size_y; y++) {
		for (int x = 0; x < _height_map.size_x; x++) {
			TgenSetTileHeight(TileXY(x, y), Clamp(H2I(_height_map.height(x, y)), 0, max_height));
		}
	}

	IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	FreeHeightMap();
	GenerateWorldSetAbortCallback(nullptr);
}
