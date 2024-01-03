/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cheat_type.h Types related to cheating. */

#ifndef CHEAT_TYPE_H
#define CHEAT_TYPE_H

/**
 * Info about each of the cheats.
 */
struct Cheat {
	bool been_used; ///< has this cheat been used before?
	bool value;     ///< tells if the bool cheat is active or not
};

/**
 * WARNING! Do _not_ remove entries in Cheats struct or change the order
 * of the existing ones! Would break downward compatibility.
 * Only add new entries at the end of the struct!
 */
struct Cheats {
	Cheat magic_bulldozer;  ///< dynamite industries, objects
	Cheat switch_company;   ///< change to another company
	Cheat money;            ///< get rich or poor
	Cheat crossing_tunnels; ///< allow tunnels that cross each other
	Cheat no_jetcrash;      ///< no jet will crash on small airports anymore
	Cheat change_date;      ///< changes date ingame
	Cheat setup_prod;       ///< setup raw-material production in game
	Cheat edit_max_hl;      ///< edit the maximum heightlevel; this is a cheat because of the fact that it needs to reset NewGRF game state and doing so as a simple configuration breaks the expectation of many
	Cheat station_rating;   ///< Fix station ratings at 100%
};

extern Cheats _cheats;

#endif /* CHEAT_TYPE_H */
