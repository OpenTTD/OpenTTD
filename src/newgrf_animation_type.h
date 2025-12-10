/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_animation_type.h Definitions related to NewGRF animation. */

#ifndef NEWGRF_ANIMATION_TYPE_H
#define NEWGRF_ANIMATION_TYPE_H

enum class AnimationStatus : uint8_t {
	NonLooping = 0x00, ///< Animation is not looping.
	Looping = 0x01, ///< Animation is looping.
	NoAnimation = 0xFF, ///< There is no animation.
};

/** Information about animation. */
template <class AnimationTriggers>
struct AnimationInfo {
	uint8_t frames = 0; ///< The number of frames.
	AnimationStatus status = AnimationStatus::NoAnimation; ///< Status.
	uint8_t speed = 2; ///< The speed: time between frames = 2^speed ticks.
	AnimationTriggers triggers; ///< The enabled animation triggers.
};

template <>
struct AnimationInfo<void> {
	uint8_t frames = 0;
	AnimationStatus status = AnimationStatus::NoAnimation;
	uint8_t speed = 2;
};

#endif /* NEWGRF_ANIMATION_TYPE_H */
