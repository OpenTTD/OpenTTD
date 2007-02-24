/* $Id$ */

#ifndef CMD_HELPER_H
#define CMD_HELPER_H

#include "direction.h"
#include "macros.h"
#include "road.h"


template<uint N> static inline void ExtractValid();
template<> static inline void ExtractValid<1>() {}


template<typename T> struct ExtractBits;
template<> struct ExtractBits<Axis>          { static const uint Count =  1; };
template<> struct ExtractBits<DiagDirection> { static const uint Count =  2; };
template<> struct ExtractBits<RoadBits>      { static const uint Count =  4; };


template<typename T, uint N, typename U> static inline T Extract(U v)
{
	// Check if there are enough bits in v
	ExtractValid<N + ExtractBits<T>::Count <= sizeof(U) * 8>();
	return (T)GB(v, N, ExtractBits<T>::Count);
}

#endif
