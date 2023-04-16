#include "stdafx.h"
#include "genworld.h"
#include "landscape_type.h"
#include "core/bitmath_func.hpp"
#include "core/math_func.hpp"
#include <algorithm>
#include <math.h>

__attribute__((import_module("env"), import_name("IncreaseGeneratingWorldProgress"))) void IncreaseGeneratingWorldProgress(GenWorldProgress);
__attribute__((import_module("env"), import_name("MapSizeX"))) uint MapSizeX();
__attribute__((import_module("env"), import_name("MapSizeY"))) uint MapSizeY();
__attribute__((import_module("env"), import_name("MapLogX"))) uint MapLogX();
__attribute__((import_module("env"), import_name("MapLogY"))) uint MapLogY();
__attribute__((import_module("env"), import_name("RandomRange"))) uint RandomRange(uint);
__attribute__((import_module("env"), import_name("SetTileHeight"))) void SetTileHeight(uint, uint);
__attribute__((import_module("env"), import_name("IsInnerTile"))) bool IsInnerTile(uint);
__attribute__((import_module("env"), import_name("MakeClear"))) void MakeClear(uint, uint, uint);

/** Fixed point type for heights */
typedef int16 height_t;
static const int height_decimal_bits = 4;

/** Fixed point array for amplitudes (and percent values) */
typedef int amplitude_t;
static const int amplitude_decimal_bits = 10;

struct HeightMap
{
	std::vector<height_t> h; //< array of heights
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
	inline height_t &height(uint x, uint y)
	{
		return h[x + y * dim_x];
	}
};

static HeightMap _height_map = { {}, 0, 0, 0 };

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

/** Maximum number of TGP noise frequencies. */
static const int MAX_TGP_FREQUENCIES = 10;

/** Desired water percentage (100% == 1024) - indexed by _settings_game.difficulty.quantity_sea_lakes */
static const amplitude_t _water_percent[4] = {70, 170, 270, 420};

static const uint MIN_MAP_SIZE_BITS = 6;                       ///< Minimal size of map is equal to 2 ^ MIN_MAP_SIZE_BITS
static const uint MAX_MAP_SIZE_BITS = 12;                      ///< Maximal size of map is equal to 2 ^ MAX_MAP_SIZE_BITS

static inline bool AllocHeightMap()
{
	_height_map.size_x = MapSizeX();
	_height_map.size_y = MapSizeY();

	/* Allocate memory block for height map row pointers */
	size_t total_size = static_cast<size_t>(_height_map.size_x + 1) * (_height_map.size_y + 1);
	_height_map.dim_x = _height_map.size_x + 1;
	_height_map.h.resize(total_size);

	return true;
}

static inline void FreeHeightMap()
{
	_height_map.h.clear();
}

static inline height_t RandomHeight(amplitude_t rMax)
{
	/* Spread height into range -rMax..+rMax */
	return A2H(RandomRange(2 * rMax + 1) - rMax);
}

static height_t TGPGetMaxHeight()
{
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

	int map_size_bucket = std::min(MapLogX(), MapLogY()) - MIN_MAP_SIZE_BITS;
	int max_height_from_table = max_height[0][map_size_bucket];

	return I2H(max_height_from_table);
}

static amplitude_t GetAmplitude(int frequency)
{
	/* Base noise amplitudes (multiplied by 1024) and indexed by "smoothness setting" and log2(frequency). */
	static const amplitude_t amplitudes[][7] = {
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

	int smoothness = 0; //_settings_game.game_creation.tgen_smoothness;

	/* Get the table index, and return that value if possible. */
	int index = frequency - MAX_TGP_FREQUENCIES + lengthof(amplitudes[smoothness]);
	amplitude_t amplitude = amplitudes[smoothness][std::max(0, index)];
	if (index >= 0) return amplitude;

	/* We need to extrapolate the amplitude. */
	double extrapolation_factor = extrapolation_factors[smoothness];
	int height_range = I2H(16);
	do {
		amplitude = (amplitude_t)(extrapolation_factor * (double)amplitude);
		height_range <<= 1;
		index++;
	} while (index < 0);

	return Clamp((TGPGetMaxHeight() - height_range) / height_range, 0, 1) * amplitude;
}

static void HeightMapGenerate()
{
	/* Trying to apply noise to uninitialized height map */
	assert(!_height_map.h.empty());

	int start = std::max(MAX_TGP_FREQUENCIES - (int)std::min(MapLogX(), MapLogY()), 0);
	bool first = true;

	for (int frequency = start; frequency < MAX_TGP_FREQUENCIES; frequency++) {
		const amplitude_t amplitude = GetAmplitude(frequency);

		/* Ignore zero amplitudes; it means our map isn't height enough for this
		 * amplitude, so ignore it and continue with the next set of amplitude. */
		if (amplitude == 0) continue;

		const int step = 1 << (MAX_TGP_FREQUENCIES - frequency - 1);

		if (first) {
			/* This is first round, we need to establish base heights with step = size_min */
			for (int y = 0; y <= _height_map.size_y; y += step) {
				for (int x = 0; x <= _height_map.size_x; x += step) {
					height_t height = (amplitude > 0) ? RandomHeight(amplitude) : 0;
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
				height_t h00 = _height_map.height(x + 0 * step, y);
				height_t h02 = _height_map.height(x + 2 * step, y);
				height_t h01 = (h00 + h02) / 2;
				_height_map.height(x + 1 * step, y) = h01;
			}
		}

		/* Interpolate height values at odd y tiles */
		for (int y = 0; y <= _height_map.size_y - 2 * step; y += 2 * step) {
			for (int x = 0; x <= _height_map.size_x; x += step) {
				height_t h00 = _height_map.height(x, y + 0 * step);
				height_t h20 = _height_map.height(x, y + 2 * step);
				height_t h10 = (h00 + h20) / 2;
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

static void HeightMapGetMinMaxAvg(height_t *min_ptr, height_t *max_ptr, height_t *avg_ptr)
{
	height_t h_min, h_max, h_avg;
	int64 h_accu = 0;
	h_min = h_max = _height_map.height(0, 0);

	/* Get h_min, h_max and accumulate heights into h_accu */
	for (const height_t &h : _height_map.h) {
		if (h < h_min) h_min = h;
		if (h > h_max) h_max = h;
		h_accu += h;
	}

	/* Get average height */
	h_avg = (height_t)(h_accu / (_height_map.size_x * _height_map.size_y));

	/* Return required results */
	if (min_ptr != nullptr) *min_ptr = h_min;
	if (max_ptr != nullptr) *max_ptr = h_max;
	if (avg_ptr != nullptr) *avg_ptr = h_avg;
}

static int *HeightMapMakeHistogram(height_t h_min, height_t h_max, int *hist_buf)
{
	int *hist = hist_buf - h_min;

	/* Count the heights and fill the histogram */
	for (const height_t &h : _height_map.h){
		assert(h >= h_min);
		assert(h <= h_max);
		hist[h]++;
	}
	return hist;
}

static void HeightMapAdjustWaterLevel(amplitude_t water_percent, height_t h_max_new)
{
	height_t h_min, h_max, h_avg, h_water_level;
	int64 water_tiles, desired_water_tiles;
	int *hist;

	HeightMapGetMinMaxAvg(&h_min, &h_max, &h_avg);

	/* Allocate histogram buffer and clear its cells */
	int *hist_buf = new int[h_max - h_min + 1];
	/* Fill histogram */
	hist = HeightMapMakeHistogram(h_min, h_max, hist_buf);

	/* How many water tiles do we want? */
	desired_water_tiles = A2I(((int64)water_percent) * (int64)(_height_map.size_x * _height_map.size_y));

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
	for (height_t &h : _height_map.h) {
		/* Transform height from range h_water_level..h_max into 0..h_max_new range */
		h = (height_t)(((int)h_max_new) * (h - h_water_level) / (h_max - h_water_level)) + I2H(1);
		/* Make sure all values are in the proper range (0..h_max_new) */
		if (h < 0) h = I2H(0);
		if (h >= h_max_new) h = h_max_new - 1;
	}

	delete[] hist_buf;
}

static uint TileXY(int x, int y)
{
	return (y << MapLogX()) + x;
}

static double int_noise(const long x, const long y, const int prime)
{
	long n = x + y * prime + 12345; //_settings_game.game_creation.generation_seed;

	n = (n << 13) ^ n;

	/* Pseudo-random number generator, using several large primes */
	return 1.0 - (double)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0;
}

static inline double linear_interpolate(const double a, const double b, const double x)
{
	return a + x * (b - a);
}

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

static void HeightMapCoastLines(uint8 water_borders)
{
	int smallest_size = std::min(MapLogX(), MapLogY());
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

static void HeightMapSmoothSlopes(height_t dh_max)
{
	for (int y = 0; y <= (int)_height_map.size_y; y++) {
		for (int x = 0; x <= (int)_height_map.size_x; x++) {
			height_t h_max = std::min(_height_map.height(x > 0 ? x - 1 : x, y), _height_map.height(x, y > 0 ? y - 1 : y)) + dh_max;
			if (_height_map.height(x, y) > h_max) _height_map.height(x, y) = h_max;
		}
	}
	for (int y = _height_map.size_y; y >= 0; y--) {
		for (int x = _height_map.size_x; x >= 0; x--) {
			height_t h_max = std::min(_height_map.height(x < _height_map.size_x ? x + 1 : x, y), _height_map.height(x, y < _height_map.size_y ? y + 1 : y)) + dh_max;
			if (_height_map.height(x, y) > h_max) _height_map.height(x, y) = h_max;
		}
	}
}

static inline bool IsValidXY(int x, int y)
{
	return x >= 0 && x < _height_map.size_x && y >= 0 && y < _height_map.size_y;
}

static void HeightMapSmoothCoastInDirection(int org_x, int org_y, int dir_x, int dir_y)
{
	const int max_coast_dist_from_edge = 35;
	const int max_coast_Smooth_depth = 35;

	int x, y;
	int ed; // coast distance from edge
	int depth;

	height_t h_prev = I2H(1);
	height_t h;

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
		h = std::min<uint>(h, h_prev + (4 + depth)); // coast softening formula
		_height_map.height(x, y) = h;
		h_prev = h;
	}
}

static void HeightMapSmoothCoasts(uint8 water_borders)
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

static void HeightMapSineTransform(height_t h_min, height_t h_max)
{
	for (height_t &h : _height_map.h) {
		double fheight;

		if (h < h_min) continue;

		/* Transform height into 0..1 space */
		fheight = (double)(h - h_min) / (double)(h_max - h_min);
		/* Apply sine transform depending on landscape type */
		/* Move and scale 0..1 into -1..+1 */
		fheight = 2 * fheight - 1;
		/* Sine transform */
		fheight = sin(fheight * M_PI_2);
		/* Transform it back from -1..1 into 0..1 space */
		fheight = 0.5 * (fheight + 1);

		/* Transform it back into h_min..h_max space */
		h = (height_t)(fheight * (h_max - h_min) + h_min);
		if (h < 0) h = I2H(0);
		if (h >= h_max) h = h_max - 1;
	}
}

static void HeightMapNormalize()
{
	int sea_level_setting = 1; //_settings_game.difficulty.quantity_sea_lakes;
	const amplitude_t water_percent = _water_percent[sea_level_setting];
	const height_t h_max_new = TGPGetMaxHeight();
	const height_t roughness = 7 + 3 * 1; //_settings_game.game_creation.tgen_smoothness;

	HeightMapAdjustWaterLevel(water_percent, h_max_new);

	byte water_borders = 0xf; //_settings_game.construction.freeform_edges ? _settings_game.game_creation.water_borders : 0xF;
	// if (water_borders == BORDERS_RANDOM) water_borders = GB(Random(), 0, 4);

	HeightMapCoastLines(water_borders);
	HeightMapSmoothSlopes(roughness);

	HeightMapSmoothCoasts(water_borders);
	HeightMapSmoothSlopes(roughness);

	HeightMapSineTransform(I2H(1), h_max_new);

	// if (_settings_game.game_creation.variety > 0) {
	// 	HeightMapCurves(_settings_game.game_creation.variety);
	// }

	HeightMapSmoothSlopes(I2H(1));
}

static void TgenSetTileHeight(uint tile, int height)
{
	SetTileHeight(tile, height);

	/* Only clear the tiles within the map area. */
	if (IsInnerTile(tile)) {
		MakeClear(tile, 0, 3);
	}
}

extern "C" {

void generate_terrain()
{
	if (!AllocHeightMap()) return;

	HeightMapGenerate();

	// IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	HeightMapNormalize();

	// IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	int max_height = H2I(TGPGetMaxHeight());

	for (int y = 0; y < _height_map.size_y; y++) {
		for (int x = 0; x < _height_map.size_x; x++) {
			TgenSetTileHeight(TileXY(x, y), Clamp(H2I(_height_map.height(x, y)), 0, max_height));
		}
	}

	// IncreaseGeneratingWorldProgress(GWP_LANDSCAPE);

	FreeHeightMap();
}

}

int main() { return 0; }
