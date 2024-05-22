/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signal_type.h Types and classes related to signals. */

#ifndef SIGNAL_TYPE_H
#define SIGNAL_TYPE_H

#include "core/enum_type.hpp"

/** Variant of the signal, i.e. how does the signal look? */
enum SignalVariant : uint8_t {
	SIG_ELECTRIC  = 0, ///< Light signal
	SIG_SEMAPHORE = 1, ///< Old-fashioned semaphore signal
};


/** Type of signal, i.e. how does the signal behave? */
enum SignalType : uint8_t {
	SIGTYPE_BLOCK      = 0, ///< block signal
	SIGTYPE_ENTRY      = 1, ///< presignal block entry
	SIGTYPE_EXIT       = 2, ///< presignal block exit
	SIGTYPE_COMBO      = 3, ///< presignal inter-block
	SIGTYPE_PBS        = 4, ///< normal pbs signal
	SIGTYPE_PBS_ONEWAY = 5, ///< no-entry signal

	SIGTYPE_END,
	SIGTYPE_LAST       = SIGTYPE_PBS_ONEWAY,
	SIGTYPE_LAST_NOPBS = SIGTYPE_COMBO,
};
DECLARE_ENUM_AS_ADDABLE(SignalType)

/**
 * These are states in which a signal can be. Currently these are only two, so
 * simple boolean logic will do. But do try to compare to this enum instead of
 * normal boolean evaluation, since that will make future additions easier.
 */
enum SignalState {
	SIGNAL_STATE_RED   = 0, ///< The signal is red
	SIGNAL_STATE_GREEN = 1, ///< The signal is green
};

#endif /* SIGNAL_TYPE_H */
