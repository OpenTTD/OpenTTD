/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file tgp.cpp OTTD Perlin Noise Landscape Generator, aka TerraGenesis Perlin.
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
 * Have a map of 4x4 tiles, our simplified noise generator produces only two
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

#include "stdafx.h"
#include "clear_map.h"
#include "void_map.h"
#include "genworld.h"
#include "core/random_func.hpp"
#include "landscape_type.h"

#include "safeguards.h"

/** Fixed point type for heights */
using Height = int16_t;
static const int HEIGHT_DECIMAL_BITS = 4;

/** Fixed point array for amplitudes */
using Amplitude = int;
static const int AMPLITUDE_DECIMAL_BITS = 10;

/** Height map - allocated array of heights (Map::SizeX() + 1) * (Map::SizeY() + 1) */
struct HeightMap
{
	std::vector<Height> h; ///< array of heights
	/* Even though the sizes are always positive, there are many cases where
	 * X and Y need to be signed integers due to subtractions. */
	int      dim_x;      ///< height map size_x Map::SizeX() + 1
	int      size_x;     ///< Map::SizeX()
	int      size_y;     ///< Map::SizeY()

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

/**
 * Convert tile height to fixed point Height.
 * @param i The tile height.
 * @return The converted fixed point height.
 */
static Height I2H(int i)
{
	return i << HEIGHT_DECIMAL_BITS;
}

/**
 * Convert fixed point Height to tile height.
 * @param i The fixed point Height.
 * @return The tile height.
 */
static int H2I(Height i)
{
	return i >> HEIGHT_DECIMAL_BITS;
}

/**
 * Convert Amplitude to fixed point Height.
 * @param i The amplitude.
 * @return The converted fixed point height.
 */
static Height A2H(Amplitude i)
{
	return i >> (AMPLITUDE_DECIMAL_BITS - HEIGHT_DECIMAL_BITS);
}

/** Maximum number of TGP noise frequencies. */
static const int MAX_TGP_FREQUENCIES = 10;

static constexpr int WATER_PERCENT_FACTOR = 1024;

/** Desired water percentage (100% == 1024) - indexed by _settings_game.difficulty.quantity_sea_lakes */
static const int64_t _water_percent[4] = {70, 170, 270, 420};

/**
 * Gets the maximum allowed height while generating a map based on
 * mapsize, terraintype, and the maximum height level.
 * @return The maximum height for the map generation.
 * @note Values should never be lower than 3 since the minimum snowline height is 2.
 */
static Height TGPGetMaxHeight()
{
	if (_settings_game.difficulty.terrain_type == GenworldMaxHeight::Custom) {
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
	int max_height_from_table = max_height[to_underlying(_settings_game.difficulty.terrain_type)][map_size_bucket];

	/* If there is a manual map height limit, clamp to it. */
	if (_settings_game.construction.map_height_limit != 0) {
		max_height_from_table = std::min<uint>(max_height_from_table, _settings_game.construction.map_height_limit);
	}

	return I2H(max_height_from_table);
}

/**
 * Get an overestimation of the highest peak TGP wants to generate.
 * @return The estimated map tile height.
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
	int index = frequency - MAX_TGP_FREQUENCIES + static_cast<int>(std::size(amplitudes[smoothness]));
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
 * Allocate array of (Map::SizeX() + 1) * (Map::SizeY() + 1) heights and init the _height_map structure members
 */
static inline void AllocHeightMap()
{
	assert(_height_map.h.empty());

	_height_map.size_x = Map::SizeX();
	_height_map.size_y = Map::SizeY();

	/* Allocate memory block for height map row pointers */
	size_t total_size = static_cast<size_t>(_height_map.size_x + 1) * (_height_map.size_y + 1);
	_height_map.dim_x = _height_map.size_x + 1;
	_height_map.h.resize(total_size);
}

/** Free height map */
static inline void FreeHeightMap()
{
	_height_map.h.clear();
}

/**
 * Generates new random height in given amplitude (generated numbers will range from - amplitude to + amplitude)
 * @param r_max Limit of result
 * @return generated height
 */
static inline Height RandomHeight(Amplitude r_max)
{
	/* Spread height into range -r_max..+r_max */
	return A2H(RandomRange(2 * r_max + 1) - r_max);
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

/**
 * Create histogram and return pointer to its base point - to the count of zero heights.
 * @param h_min The minimum height for the histogram.
 * @param h_max The maximum height of the map.
 * @param hist_buf The buffer of the histogram.
 * @return Pointer to the base of the histogram.
 */
static int *HeightMapMakeHistogram(Height h_min, [[maybe_unused]] Height h_max, int *hist_buf)
{
	int *hist = hist_buf - h_min;

	/* Count the heights and fill the histogram */
	for (const Height &h : _height_map.h) {
		assert(h >= h_min);
		assert(h <= h_max);
		hist[h]++;
	}
	return hist;
}

/**
 * Adjust the landscape to create lowlands on average (tropic landscape).
 * @param fheight The height to adjust.
 * @return The adjusted height.
 */
static double SineTransformLowlands(double fheight)
{
	double height = fheight;

	/* Half of tiles should be at lowest (0..25%) heights */
	double sine_lower_limit = 0.5;
	double linear_compression = 2;
	if (height <= sine_lower_limit) {
		/* Under the limit we do linear compression down */
		height = height / linear_compression;
	} else {
		double m = sine_lower_limit / linear_compression;
		/* Get sine_lower_limit..1 into -1..1 */
		height = 2.0 * ((height - sine_lower_limit) / (1.0 - sine_lower_limit)) - 1.0;
		/* Sine wave transform */
		height = sin(height * M_PI_2);
		/* Get -1..1 back to (sine_lower_limit / linear_compression)..1.0 */
		height = 0.5 * ((1.0 - m) * height + (1.0 + m));
	}

	return height;
}

/**
 * Adjust the landscape to create normal average height (temperate and toyland landscapes).
 * @param fheight The height to adjust.
 * @return The adjusted height.
 */
static double SineTransformNormal(double &fheight)
{
	double height = fheight;

	/* Move and scale 0..1 into -1..+1 */
	height = 2 * height - 1;
	/* Sine transform */
	height = sin(height * M_PI_2);
	/* Transform it back from -1..1 into 0..1 space */
	height = 0.5 * (height + 1);

	return height;
}

/**
 * Adjust the landscape to create plateaus on average (arctic landscape).
 * @param fheight The height to adjust.
 * @return The adjusted height.
 */
static double SineTransformPlateaus(double &fheight)
{
	double height = fheight;

	/* Redistribute heights to have more tiles at highest (75%..100%) range */
	double sine_upper_limit = 0.75;
	double linear_compression = 2;
	if (height >= sine_upper_limit) {
		/* Over the limit we do linear compression up */
		height = 1.0 - (1.0 - height) / linear_compression;
	} else {
		double m = 1.0 - (1.0 - sine_upper_limit) / linear_compression;
		/* Get 0..sine_upper_limit into -1..1 */
		height = 2.0 * height / sine_upper_limit - 1.0;
		/* Sine wave transform */
		height = sin(height * M_PI_2);
		/* Get -1..1 back to 0..(1 - (1 - sine_upper_limit) / linear_compression) == 0.0..m */
		height = 0.5 * (height + 1.0) * m;
	}

	return height;
}

/**
 * Applies sine wave redistribution onto height map.
 * @param h_min The minimum height for the map.
 * @param h_max The maximum height for the map.
 */
static void HeightMapSineTransform(Height h_min, Height h_max)
{
	for (Height &h : _height_map.h) {
		double fheight;

		if (h < h_min) continue;

		/* Transform height into 0..1 space */
		fheight = (double)(h - h_min) / (double)(h_max - h_min);

		switch (_settings_game.game_creation.average_height) {
			case GenworldAverageHeight::Auto:
				/* Apply sine transform depending on landscape type */
				switch (_settings_game.game_creation.landscape) {
					case LandscapeType::Temperate: fheight = SineTransformNormal(fheight); break;
					case LandscapeType::Tropic: fheight = SineTransformLowlands(fheight); break;
					case LandscapeType::Arctic: fheight = SineTransformPlateaus(fheight); break;
					case LandscapeType::Toyland: fheight = SineTransformNormal(fheight); break;
					default: NOT_REACHED();
				}
				break;

			case GenworldAverageHeight::Lowlands: fheight = SineTransformLowlands(fheight); break;
			case GenworldAverageHeight::Normal: fheight = SineTransformNormal(fheight); break;
			case GenworldAverageHeight::Plateaus: fheight = SineTransformPlateaus(fheight); break;
			default: NOT_REACHED();
		}

		/* Transform it back into h_min..h_max space */
		h = static_cast<Height>(fheight * (h_max - h_min) + h_min);
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

	const std::span<const ControlPoint> curve_maps[] = { curve_map_1, curve_map_2, curve_map_3, curve_map_4 };

	std::array<Height, std::size(curve_maps)> ht{};

	/* Set up a grid to choose curve maps based on location; attempt to get a somewhat square grid */
	float factor = sqrt((float)_height_map.size_x / (float)_height_map.size_y);
	uint sx = Clamp((int)(((1 << level) * factor) + 0.5), 1, 128);
	uint sy = Clamp((int)(((1 << level) / factor) + 0.5), 1, 128);
	std::vector<uint8_t> c(static_cast<size_t>(sx) * sy);

	for (uint i = 0; i < sx * sy; i++) {
		c[i] = RandomRange(static_cast<uint32_t>(std::size(curve_maps)));
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
			for (size_t t = 0; t < std::size(curve_maps); t++) {
				if (!HasBit(corner_bits, static_cast<uint8_t>(t))) continue;

				[[maybe_unused]] bool found = false;
				auto &cm = curve_maps[t];
				for (size_t i = 0; i < cm.size() - 1; i++) {
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

			/* Re-add sea level */
			*h += I2H(1);
		}
	}
}

/**
 * Adjusts heights in height map to contain required amount of water tiles.
 * @param water_percent Fixed point water percentage on the map.
 * @param h_max_new The new maximum height.
 */
static void HeightMapAdjustWaterLevel(int64_t water_percent, Height h_max_new)
{
	Height h_water_level;
	int64_t water_tiles, desired_water_tiles;
	int *hist;

	auto [h_min, h_max] = std::ranges::minmax(_height_map.h);

	/* Allocate histogram buffer and clear its cells */
	std::vector<int> hist_buf(h_max - h_min + 1);
	/* Fill histogram */
	hist = HeightMapMakeHistogram(h_min, h_max, hist_buf.data());

	/* How many water tiles do we want? */
	desired_water_tiles = water_percent * _height_map.size_x * _height_map.size_y / WATER_PERCENT_FACTOR;

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
}

static double PerlinCoastNoise2D(const double x, const double y, const double p, const int prime);

/**
 * This routine sculpts in from the edge a random amount, again from Perlin
 * sequences, to avoid rigid flat-edge slopes and add an irregular edge to the
 * land. The smoothing routines makes it legal, gradually increasing up from
 * the edge to the original terrain.
 *
 * More varied coastlines on large maps require coastal features that would
 * turn small maps into tiny islands. To avoid this, multiple perlin sequences
 * with different persistence values are scaled based on the map dimensions.
 *
 * The constants used are based on what looks right. If you're changing the
 * limits or the persistence values you'll want to experiment with various map
 * sizes.
 * @param water_borders The sides with water borders.
 */
static void HeightMapCoastLines(BorderFlags water_borders)
{
	/* Both map dimensions are powers of 2. Division by smaller powers of 2 will always result in an integer. */
	const int smallest_size = std::min(_height_map.size_x, _height_map.size_y);
	const int map_ratio = std::max(_height_map.size_x, _height_map.size_y) / smallest_size;

	/* More jagged perlin noise is used to create inlets and craggier features. It scales with map size and ratio and quickly
	 * reaches the limit of 64. It scales both by the shortest side and the map ratio to try and balance variation in the
	 * coastline and the amount of water on the map.
	 */
	const int jagged_distance = std::min(12 + (smallest_size * smallest_size / 4096) + std::min(map_ratio, 16), 64);

	/* Smoother perlin noise is used to add more depth to the coastline as the smallest edge increases in length */
	const int smooth_distance = std::min(smallest_size / 32, 32);

	/* Function to get the distance from the edge at x (the coastline is actually 1D noise).
	 * p1, p2, p3 are small prime numbers so the sequences aren't identical.
	 */
	auto get_depth = [&](int x, int p1, int p2, int p3) {
		return 2 // we ensure a margin of two water tiles at the edge of the map
			+ smooth_distance * (1 + PerlinCoastNoise2D(x, x, 0.2, p1)) // +1 rather than abs reduces the number of V shaped inlets
			+ jagged_distance * abs(PerlinCoastNoise2D(x, x, 0.5, p2))
			+ 8 * abs(PerlinCoastNoise2D(x, x, 0.8, p3)); // Some unscaled jaggedness to breakup anything smoothed by scaling
	};

	int y, x;

	/* Lower to sea level */
	for (y = 0; y <= _height_map.size_y; y++) {
		if (water_borders.Test(BorderFlag::NorthEast)) {
			/* Top right */
			for (x = 0; x < get_depth(y, 67, 179, 53); x++) {
				_height_map.height(x, y) = 0;
			}
		}

		if (water_borders.Test(BorderFlag::SouthWest)) {
			/* Bottom left */
			for (x = _height_map.size_x; x > (_height_map.size_x - 1 - get_depth(y, 199, 67, 101)); x--) {
				_height_map.height(x, y) = 0;
			}
		}
	}

	/* Lower to sea level */
	for (x = 0; x <= _height_map.size_x; x++) {
		if (water_borders.Test(BorderFlag::NorthWest)) {
			/* Top left */
			for (y = 0; y < get_depth(x, 179, 211, 167); y++) {
				_height_map.height(x, y) = 0;
			}
		}

		if (water_borders.Test(BorderFlag::SouthEast)) {
			/* Bottom right */
			for (y = _height_map.size_y; y > (_height_map.size_y - 1 - get_depth(x, 101, 193, 71)); y--) {
				_height_map.height(x, y) = 0;
			}
		}
	}
}

/**
 * Start at given point, move in given direction, find and Smooth coast in that direction.
 * @param org_x The X-coordinate to start at.
 * @param org_y The Y-coordinate to start at.
 * @param dir_x The direction along the X-axis.
 * @param dir_y The direction along the Y-axis.
 */
static void HeightMapSmoothCoastInDirection(int org_x, int org_y, int dir_x, int dir_y)
{
	const int max_coast_dist_from_edge = 100;
	const int max_coast_smooth_depth = 35;

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

		/* Coast found in the neighbourhood? */
		if (IsValidXY(x + dir_y, y + dir_x) && _height_map.height(x + dir_y, y + dir_x) > 0) break;

		/* Coast found in the neighbourhood on the other side */
		if (IsValidXY(x - dir_y, y - dir_x) && _height_map.height(x - dir_y, y - dir_x) > 0) break;
	}

	/* Coast found or max_coast_dist_from_edge has been reached.
	 * Soften the coast slope */
	for (depth = 0; IsValidXY(x, y) && depth <= max_coast_smooth_depth; depth++, x += dir_x, y += dir_y) {
		h = _height_map.height(x, y);
		h = static_cast<Height>(std::min<uint>(h, h_prev + (4 + depth))); // coast softening formula
		_height_map.height(x, y) = h;
		h_prev = h;
	}
}

/**
 * Smooth coasts by modulating height of tiles close to map edges with cosine of distance from edge.
 * @param water_borders The sides where the borders are water.
 */
static void HeightMapSmoothCoasts(BorderFlags water_borders)
{
	int x, y;
	/* First Smooth NW and SE coasts (y close to 0 and y close to size_y) */
	for (x = 0; x < _height_map.size_x; x++) {
		if (water_borders.Test(BorderFlag::NorthWest)) HeightMapSmoothCoastInDirection(x, 0, 0, 1);
		if (water_borders.Test(BorderFlag::SouthEast)) HeightMapSmoothCoastInDirection(x, _height_map.size_y - 1, 0, -1);
	}
	/* First Smooth NE and SW coasts (x close to 0 and x close to size_x) */
	for (y = 0; y < _height_map.size_y; y++) {
		if (water_borders.Test(BorderFlag::NorthEast)) HeightMapSmoothCoastInDirection(0, y, 1, 0);
		if (water_borders.Test(BorderFlag::SouthWest)) HeightMapSmoothCoastInDirection(_height_map.size_x - 1, y, -1, 0);
	}
}

/**
 * This routine provides the essential cleanup necessary before OTTD can
 * display the terrain. When generated, the terrain heights can jump more than
 * one level between tiles. This routine smooths out those differences so that
 * the most it can change is one level. When OTTD can support cliffs, this
 * routine may not be necessary.
 * @param dh_max The maximum height different between two adjacent tiles.
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
	const int64_t water_percent = _settings_game.difficulty.quantity_sea_lakes != CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY ? _water_percent[_settings_game.difficulty.quantity_sea_lakes] : _settings_game.game_creation.custom_sea_level * WATER_PERCENT_FACTOR / 100;
	const Height h_max_new = TGPGetMaxHeight();
	const Height roughness = 7 + 3 * _settings_game.game_creation.tgen_smoothness;

	HeightMapAdjustWaterLevel(water_percent, h_max_new);

	BorderFlags water_borders = _settings_game.construction.freeform_edges ? _settings_game.game_creation.water_borders : BORDERFLAGS_ALL;
	if (water_borders == BorderFlag::Random) water_borders = static_cast<BorderFlags>(GB(Random(), 0, 4));

	HeightMapCoastLines(water_borders);
	HeightMapSmoothSlopes(roughness);

	HeightMapSmoothCoasts(water_borders);
	HeightMapSmoothSlopes(roughness);

	HeightMapSineTransform(I2H(1), h_max_new);

	if (_settings_game.game_creation.variety > 0) {
		HeightMapCurves(_settings_game.game_creation.variety);
	}
}

/**
 * The Perlin Noise calculation using large primes
 * The initial number is adjusted by two values; the generation_seed, and the
 * passed parameter; prime.
 * prime is used to allow the perlin noise generator to create useful random
 * numbers from slightly different series.
 * @param x The X-coordinate.
 * @param y The Y-coordinate.
 * @param prime The prime for the generation.
 * @return The Perlin noise.
 */
static double IntNoise(const long x, const long y, const int prime)
{
	long n = x + y * prime + _settings_game.game_creation.generation_seed;

	n = (n << 13) ^ n;

	/* Pseudo-random number generator, using several large primes */
	return 1.0 - (double)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0;
}


/**
 * This routine determines the interpolated value between a and b
 * @param a The first value.
 * @param b The second value.
 * @param x The fraction between the two values to get the value for.
 * @return The interpolated value.
 */
static inline double LinearInterpolate(const double a, const double b, const double x)
{
	return a + x * (b - a);
}


/**
 * This routine returns the smoothed interpolated noise for an x and y, using
 * the values from the surrounding positions.
 * @param x The X-coordinate.
 * @param y The Y-coordinate.
 * @param prime The prime for the generation.
 * @return The smoothed Perlin noise.
 */
static double InterpolatedNoise(const double x, const double y, const int prime)
{
	const int integer_x = (int)x;
	const int integer_y = (int)y;

	const double fractional_x = x - (double)integer_x;
	const double fractional_y = y - (double)integer_y;

	const double v1 = IntNoise(integer_x,     integer_y,     prime);
	const double v2 = IntNoise(integer_x + 1, integer_y,     prime);
	const double v3 = IntNoise(integer_x,     integer_y + 1, prime);
	const double v4 = IntNoise(integer_x + 1, integer_y + 1, prime);

	const double i1 = LinearInterpolate(v1, v2, fractional_x);
	const double i2 = LinearInterpolate(v3, v4, fractional_x);

	return LinearInterpolate(i1, i2, fractional_y);
}

/**
 * This is a similar function to the main perlin noise calculation, but uses
 * the value p passed as a parameter rather than selected from the predefined
 * sequences. as you can guess by its title, i use this to create the indented
 * coastline, which is just another perlin sequence.
 * @param x The X-coordinate.
 * @param y The Y-coordinate.
 * @param p Scaling factor for the noise.
 * @param prime The prime for the generation.
 * @return The smoothed Perlin noise.
 */
static double PerlinCoastNoise2D(const double x, const double y, const double p, const int prime)
{
	constexpr int OCTAVES = 6;
	constexpr double INITIAL_FREQUENCY = 1 << OCTAVES;

	double total = 0.0;
	double max_value = 0.0;
	double frequency = 1.0 / INITIAL_FREQUENCY;
	double amplitude = 1.0;
	for (int i = 0; i < OCTAVES; i++) {
		total += InterpolatedNoise(x * frequency, y * frequency, prime) * amplitude;
		max_value += amplitude;

		frequency *= 2.0;
		amplitude *= p;
	}

	/* Bringing the output range into [-1, 1] makes it much easier to reason with */
	return total / max_value;
}


/**
 * A small helper function to initialize the terrain.
 * @param tile The tile to set the height for.
 * @param height The new height of the tile.
 */
static void TgenSetTileHeight(TileIndex tile, int height)
{
	SetTileHeight(tile, height);

	/* Only clear the tiles within the map area. */
	if (IsInnerTile(tile)) {
		MakeClear(tile, ClearGround::Grass, 3);
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
	AllocHeightMap();
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

	FreeHeightMap();
	GenerateWorldSetAbortCallback(nullptr);
}
