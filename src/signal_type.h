/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signal_type.h Types and classes related to signals. */

#ifndef SIGNAL_TYPE_H
#define SIGNAL_TYPE_H

#include "core/enum_type.hpp"

/** Variant of the signal, i.e. how does the signal look? */
enum class SignalVariant : uint8_t {
	Electric = 0, ///< Light signal.
	Semaphore = 1, ///< Old-fashioned semaphore signal.
	End, ///< End marker.
};


/** Type of signal, i.e. how does the signal behave? */
enum class SignalType : uint8_t {
	Block = 0, ///< block signal.
	Entry = 1, ///< presignal block entry.
	Exit = 2, ///< presignal block exit.
	Combo = 3, ///< presignal inter-block.
	Path = 4, ///< normal path signal.
	PathOneWay = 5, ///< no-entry path signal.
	End, ///< End marker.
};
DECLARE_ENUM_AS_ADDABLE(SignalType)

/**
 * These are states in which a signal can be. Currently these are only two, so
 * simple boolean logic will do. But do try to compare to this enum instead of
 * normal boolean evaluation, since that will make future additions easier.
 */
enum SignalState : uint8_t {
	SIGNAL_STATE_RED   = 0, ///< The signal is red
	SIGNAL_STATE_GREEN = 1, ///< The signal is green
	SIGNAL_STATE_END, ///< End marker.
};

#endif /* SIGNAL_TYPE_H */
