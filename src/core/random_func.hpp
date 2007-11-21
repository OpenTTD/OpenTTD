/* $Id$ */

/** @file random_func.h */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
// #define MERSENNE_TWISTER

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif

uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

#endif /* RANDOM_FUNC_HPP */
/* $Id$ */

/** @file random_func.h */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
// #define MERSENNE_TWISTER

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif

uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

#endif /* RANDOM_FUNC_HPP */
/* $Id$ */

/** @file random_func.h */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
// #define MERSENNE_TWISTER

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif

uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

#endif /* RANDOM_FUNC_HPP */
/* $Id$ */

/** @file random_func.h */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
// #define MERSENNE_TWISTER

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif

uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

#endif /* RANDOM_FUNC_HPP */
/* $Id$ */

/** @file random_func.h */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
// #define MERSENNE_TWISTER

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif

uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

#endif /* RANDOM_FUNC_HPP */
/* $Id$ */

/** @file random_func.h */

#ifndef RANDOM_FUNC_HPP
#define RANDOM_FUNC_HPP

/**************
 * Warning: DO NOT enable this unless you understand what it does
 *
 * If enabled, in a network game all randoms will be dumped to the
 *  stdout if the first client joins (or if you are a client). This
 *  is to help finding desync problems.
 *
 * Warning: DO NOT enable this unless you understand what it does
 **************/

//#define RANDOM_DEBUG


// Enable this to produce higher quality random numbers.
// Doesn't work with network yet.
// #define MERSENNE_TWISTER

void SetRandomSeed(uint32 seed);
#ifdef RANDOM_DEBUG
	#define Random() DoRandom(__LINE__, __FILE__)
	uint32 DoRandom(int line, const char *file);
	#define RandomRange(max) DoRandomRange(max, __LINE__, __FILE__)
	uint DoRandomRange(uint max, int line, const char *file);
#else
	uint32 Random();
	uint RandomRange(uint max);
#endif

uint32 InteractiveRandom(); // Used for random sequences that are not the same on the other end of the multiplayer link
uint InteractiveRandomRange(uint max);

#endif /* RANDOM_FUNC_HPP */
