/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_network.h Definition of the game-network-timer */

#ifndef TIMER_NETWORK_H
#define TIMER_NETWORK_H

/**
 * Timer that is increased every 27ms, even for paused games.
 *
 * Mostly meant for the network, that even continues if the game is paused.
 * The reason that the network doesn't (fully) run on TimerReal, is because
 * this timer notices when the game starts to lag, and starts to slow down
 * too.
 */
class TimerNetwork {
public:
	using TPeriod = uint;
	using TElapsed = uint;
	struct TStorage {
	};
};

#endif /* TIMER_NETWORK_H */
