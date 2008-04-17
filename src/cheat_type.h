/* $Id$ */

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
	Cheat magic_bulldozer;  ///< dynamite industries, unmovables
	Cheat switch_player;    ///< change to another player
	Cheat money;            ///< get rich or poor
	Cheat crossing_tunnels; ///< allow tunnels that cross each other
	Cheat build_in_pause;   ///< build while in pause mode
	Cheat no_jetcrash;      ///< no jet will crash on small airports anymore
	Cheat switch_climate;   ///< change the climate of the map
	Cheat change_date;      ///< changes date ingame
	Cheat setup_prod;       ///< setup raw-material production in game
	Cheat dummy;            ///< empty cheat (enable running el-engines on normal rail)
};

extern Cheats _cheats;

#endif /* CHEAT_TYPE_H */
