/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file elrail_data.h Stores all the data for overhead wire and pylon drawing.
 * @see elrail.cpp
 */

#ifndef ELRAIL_DATA_H
#define ELRAIL_DATA_H

/**
 * Tile Location group.
 * This defines whether the X and or Y coordinate of a tile is even
 */
enum TileLocationGroup : uint8_t {
	XEVEN_YEVEN = 0,
	XEVEN_YODD  = 1,
	XODD_YEVEN  = 2,
	XODD_YODD   = 3,
	TLG_END
};

/**
 * When determining the pylon configuration on the edge, two tiles are taken
 * into account: the tile being drawn itself (the home tile, the one in
 * ti->tile), and the neighbouring tile
 */
enum class TileSource : uint8_t {
	Home, ///< Home tile.
	Neighbour, ///< Neighbouring tile.
	End, ///< End marker.
};

static const uint NUM_TRACKS_AT_PCP = 6;

/** Which PPPs are possible at all on a given PCP */
static const DiagDirectionIndexArray<Directions> _allowed_ppp_on_pcp{{{
	{Direction::N, Direction::E, Direction::SE, Direction::S, Direction::W, Direction::NW},
	{Direction::N, Direction::NE, Direction::E, Direction::S, Direction::SW, Direction::W},
	{Direction::N, Direction::E, Direction::SE, Direction::S, Direction::W, Direction::NW},
	{Direction::N, Direction::NE, Direction::E, Direction::S, Direction::SW, Direction::W},
}}};

/**
 * Which of the PPPs are inside the tile. For the two PPPs on the tile border
 * the following system is used: if you rotate the PCP so that it is in the
 * north, the eastern PPP belongs to the tile.
 */
static const DiagDirectionIndexArray<Directions> _owned_ppp_on_pcp{{{
	{Direction::SE, Direction::S, Direction::SW, Direction::W},
	{Direction::N, Direction::SW, Direction::W, Direction::NW},
	{Direction::N, Direction::NE, Direction::E, Direction::NW},
	{Direction::NE, Direction::E, Direction::SE, Direction::S},
}}};

/** Maps a track bit onto two PCP positions */
static const TrackIndexArray<std::array<DiagDirection, 2>> _pcp_positions{{{
	{DiagDirection::NE, DiagDirection::SW}, // X
	{DiagDirection::SE, DiagDirection::NW}, // Y
	{DiagDirection::NW, DiagDirection::NE}, // UPPER
	{DiagDirection::SE, DiagDirection::SW}, // LOWER
	{DiagDirection::SW, DiagDirection::NW}, // LEFT
	{DiagDirection::NE, DiagDirection::SE}, // RIGHT
}}};

/**
 * Preferred points of each trackbit. Those are the ones perpendicular to the
 * track, plus the point in extension of the track (to mark end-of-track). PCPs
 * which are not on either end of the track are fully preferred.
 * @see PCPpositions
 */
static const TrackIndexArray<DiagDirectionIndexArray<Directions>> _preferred_ppp_of_track_at_pcp{{{
	{{{ // X
		{Direction::NE, Direction::SE, Direction::NW}, // NE
		DIRECTIONS_ALL,           // SE
		{Direction::SE, Direction::SW, Direction::NW}, // SW
		DIRECTIONS_ALL            // NE
	}}},
	{{{ // Y
		DIRECTIONS_ALL,
		{Direction::NE, Direction::SE, Direction::SW},
		DIRECTIONS_ALL,
		{Direction::SW, Direction::NW, Direction::NE},
	}}},
	{{{ // UPPER
		{Direction::E, Direction::N, Direction::S},
		DIRECTIONS_ALL,
		DIRECTIONS_ALL,
		{Direction::W, Direction::N, Direction::S},
	}}},
	{{{ // LOWER
		DIRECTIONS_ALL,
		{Direction::E, Direction::N, Direction::S},
		{Direction::W, Direction::N, Direction::S},
		DIRECTIONS_ALL,
	}}},
	{{{ // LEFT
		DIRECTIONS_ALL,
		DIRECTIONS_ALL,
		{Direction::S, Direction::E, Direction::W},
		{Direction::N, Direction::E, Direction::W},
	}}},
	{{{ // RIGHT
		{Direction::N, Direction::E, Direction::W},
		{Direction::S, Direction::E, Direction::W},
		DIRECTIONS_ALL,
		DIRECTIONS_ALL,
	}}},
}}};

#define NUM_IGNORE_GROUPS 3
/**
 * In case we have a straight line, we place pylon only every two tiles,
 * so there are certain tiles which we ignore. A straight line is found if
 * we have exactly two PPPs.
 */
static const DiagDirectionIndexArray<Directions> _ignored_pcp[NUM_IGNORE_GROUPS][TLG_END] = {
	{   // Ignore group 1, X and Y tracks
		{{{     // X even, Y even
			DIRECTIONS_ALL,
			{Direction::NE, Direction::SW},
			{Direction::NW, Direction::SE},
			DIRECTIONS_ALL,
		}}},
		{{{ // X even, Y odd
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
			{Direction::NW, Direction::SE},
			{Direction::NE, Direction::SW},
		}}},
		{{{ // X odd,  Y even
			{Direction::NW, Direction::SE},
			{Direction::NE, Direction::SW},
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
		}}},
		{{{ // X odd,  Y odd
			{Direction::NW, Direction::SE},
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
			{Direction::NE, Direction::SW},
		}}},
	},
	{   // Ignore group 2, LEFT and RIGHT tracks
		{{{
			{Direction::E, Direction::W},
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
			{Direction::E, Direction::W},
		}}},
		{{{
			DIRECTIONS_ALL,
			{Direction::E, Direction::W},
			{Direction::E, Direction::W},
			DIRECTIONS_ALL,
		}}},
		{{{
			DIRECTIONS_ALL,
			{Direction::E, Direction::W},
			{Direction::E, Direction::W},
			DIRECTIONS_ALL,
		}}},
		{{{
			{Direction::E, Direction::W},
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
			{Direction::E, Direction::W},
		}}},
	},
	{   // Ignore group 3, UPPER and LOWER tracks
		{{{
			{Direction::N, Direction::S},
			{Direction::N, Direction::S},
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
		}}},
		{{{
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
			{Direction::N, Direction::S},
			{Direction::N, Direction::S},
		}}},
		{{{
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
			{Direction::N, Direction::S},
			{Direction::N, Direction::S},
		}}},
		{{{
			{Direction::N, Direction::S},
			{Direction::N, Direction::S},
			DIRECTIONS_ALL,
			DIRECTIONS_ALL,
		}}},
	}
};

/** Which pylons can definitely NOT be built */
static const TrackIndexArray<DiagDirectionIndexArray<Directions>> _disallowed_ppp_of_track_at_pcp{{{
	{Directions{Direction::SW, Direction::NE}, Directions{}, Directions{Direction::SW, Direction::NE}, Directions{}}, // X
	{Directions{}, Directions{Direction::NW, Direction::SE}, Directions{}, Directions{Direction::NW, Direction::SE}}, // Y
	{Directions{Direction::W, Direction::E}, Directions{}, Directions{}, Directions{Direction::W, Direction::E}}, // UPPER
	{Directions{}, Directions{Direction::W, Direction::E}, Directions{Direction::W, Direction::E}, Directions{}}, // LOWER
	{Directions{}, Directions{}, Directions{Direction::S, Direction::N}, Directions{Direction::N, Direction::S}}, // LEFT
	{Directions{Direction::S, Direction::N}, Directions{Direction::S, Direction::N}, Directions{}, Directions{}}, // RIGHT
}}};

/** This array stores which track bits can meet at a tile edge. */
static const DiagDirectionIndexArray<std::array<Track, NUM_TRACKS_AT_PCP>> _tracks_at_pcp{{{
	{TRACK_X, TRACK_X, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT},
	{TRACK_Y, TRACK_Y, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT},
	{TRACK_X, TRACK_X, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT},
	{TRACK_Y, TRACK_Y, TRACK_UPPER, TRACK_LOWER, TRACK_LEFT, TRACK_RIGHT},
}}};

/** Takes each of the 6 track bits from the array above and assigns it to the home tile or neighbour tile. */
static const DiagDirectionIndexArray<std::array<TileSource, NUM_TRACKS_AT_PCP>> _track_source_tile{{{
	{TileSource::Home, TileSource::Neighbour, TileSource::Home, TileSource::Neighbour, TileSource::Neighbour, TileSource::Home},
	{TileSource::Home, TileSource::Neighbour, TileSource::Neighbour, TileSource::Home, TileSource::Neighbour, TileSource::Home},
	{TileSource::Home, TileSource::Neighbour, TileSource::Neighbour, TileSource::Home, TileSource::Home, TileSource::Neighbour},
	{TileSource::Home, TileSource::Neighbour, TileSource::Home, TileSource::Neighbour, TileSource::Home, TileSource::Neighbour},
}}};

/** Several PPPs maybe exist, here they are sorted in order of preference. */
static const DiagDirectionIndexArray<std::array<DirectionIndexArray<Direction>, TLG_END>> _ppp_order{{{    //  X  -  Y
	{{ // PCP 0
		{Direction::NE, Direction::NW, Direction::SE, Direction::SW, Direction::N, Direction::E, Direction::S, Direction::W}, // evn - evn
		{Direction::NE, Direction::SE, Direction::SW, Direction::NW, Direction::S, Direction::W, Direction::N, Direction::E}, // evn - odd
		{Direction::SW, Direction::NW, Direction::NE, Direction::SE, Direction::S, Direction::W, Direction::N, Direction::E}, // odd - evn
		{Direction::SW, Direction::SE, Direction::NE, Direction::NW, Direction::N, Direction::E, Direction::S, Direction::W}, // odd - odd
	}},
	{{ // PCP 1
		{Direction::NE, Direction::NW, Direction::SE, Direction::SW, Direction::S, Direction::E, Direction::N, Direction::W}, // evn - evn
		{Direction::NE, Direction::SE, Direction::SW, Direction::NW, Direction::N, Direction::W, Direction::S, Direction::E}, // evn - odd
		{Direction::SW, Direction::NW, Direction::NE, Direction::SE, Direction::N, Direction::W, Direction::S, Direction::E}, // odd - evn
		{Direction::SW, Direction::SE, Direction::NE, Direction::NW, Direction::S, Direction::E, Direction::N, Direction::W}, // odd - odd
	}},
	{{ // PCP 2
		{Direction::NE, Direction::NW, Direction::SE, Direction::SW, Direction::S, Direction::W, Direction::N, Direction::E}, // evn - evn
		{Direction::NE, Direction::SE, Direction::SW, Direction::NW, Direction::N, Direction::E, Direction::S, Direction::W}, // evn - odd
		{Direction::SW, Direction::NW, Direction::NE, Direction::SE, Direction::N, Direction::E, Direction::S, Direction::W}, // odd - evn
		{Direction::SW, Direction::SE, Direction::NE, Direction::NW, Direction::S, Direction::W, Direction::N, Direction::E}, // odd - odd
	}},
	{{ // PCP 3
		{Direction::NE, Direction::NW, Direction::SE, Direction::SW, Direction::N, Direction::W, Direction::S, Direction::E}, // evn - evn
		{Direction::NE, Direction::SE, Direction::SW, Direction::NW, Direction::S, Direction::E, Direction::N, Direction::W}, // evn - odd
		{Direction::SW, Direction::NW, Direction::NE, Direction::SE, Direction::S, Direction::E, Direction::N, Direction::W}, // odd - evn
		{Direction::SW, Direction::SE, Direction::NE, Direction::NW, Direction::N, Direction::W, Direction::S, Direction::E}, // odd - odd
	}},
}}};

/** @{
 * Geometric placement of the PCP relative to the tile origin. */
static const DiagDirectionIndexArray<int8_t> _x_pcp_offsets{0,  8, 16, 8};
static const DiagDirectionIndexArray<int8_t> _y_pcp_offsets{8, 16,  8, 0};
/** @} */
/** @{
 * Geometric placement of the PPP relative to the PCP.*/
static const DirectionIndexArray<int8_t> _x_ppp_offsets{-2, -4, -2,  0,  2,  4,  2,  0};
static const DirectionIndexArray<int8_t> _y_ppp_offsets{-2,  0,  2,  4,  2,  0, -2, -4};
/** @} */

/**
 * Offset for pylon sprites from the base pylon sprite.
 */
enum PylonSpriteOffset : uint8_t {
	PSO_Y_NE,
	PSO_Y_SW,
	PSO_X_NW,
	PSO_X_SE,
	PSO_EW_N,
	PSO_EW_S,
	PSO_NS_W,
	PSO_NS_E,
};

/** The type of pylon to draw at each PPP. */
static const DirectionIndexArray<uint8_t> _pylon_sprites{
	PSO_EW_N,
	PSO_Y_NE,
	PSO_NS_E,
	PSO_X_SE,
	PSO_EW_S,
	PSO_Y_SW,
	PSO_NS_W,
	PSO_X_NW,
};

/**
 * Offset for wire sprites from the base wire sprite.
 */
enum WireSpriteOffset : uint8_t {
	WSO_X_SHORT,
	WSO_Y_SHORT,
	WSO_EW_SHORT,
	WSO_NS_SHORT,
	WSO_X_SHORT_DOWN,
	WSO_Y_SHORT_UP,
	WSO_X_SHORT_UP,
	WSO_Y_SHORT_DOWN,

	WSO_X_SW,
	WSO_Y_SE,
	WSO_EW_E,
	WSO_NS_S,
	WSO_X_SW_DOWN,
	WSO_Y_SE_UP,
	WSO_X_SW_UP,
	WSO_Y_SE_DOWN,

	WSO_X_NE,
	WSO_Y_NW,
	WSO_EW_W,
	WSO_NS_N,
	WSO_X_NE_DOWN,
	WSO_Y_NW_UP,
	WSO_X_NE_UP,
	WSO_Y_NW_DOWN,

	WSO_ENTRANCE_SW,
	WSO_ENTRANCE_NW,
	WSO_ENTRANCE_NE,
	WSO_ENTRANCE_SE,
};

struct SortableSpriteStruct : SpriteBounds {
	uint8_t image_offset;

	constexpr SortableSpriteStruct(uint8_t image_offset, const SpriteBounds &bounds) : SpriteBounds(bounds), image_offset(image_offset) {}
	constexpr SortableSpriteStruct(uint8_t image_offset, int8_t x_offset, int8_t y_offset, uint8_t x_size, uint8_t y_size, uint8_t z_size, int8_t z_offset) :
		SpriteBounds({x_offset, y_offset, z_offset}, {x_size, y_size, z_size}, {}), image_offset(image_offset) {}
};

/** Distance between wire and rail */
static const uint ELRAIL_ELEVATION = 10;
/** Wires that a draw one level higher than the north corner. */
static const uint ELRAIL_ELEVRAISE = ELRAIL_ELEVATION + TILE_HEIGHT + 1;
/** Wires that a draw one level lower than the north corner. */
static const uint ELRAIL_ELEVLOWER = ELRAIL_ELEVATION - 1;

static const SortableSpriteStruct _rail_catenary_sprite_data[] = {
/* X direction
	 * Flat tiles:
		 * Wires */
	{ WSO_X_SW,          0,  7, 15,  1,  1, ELRAIL_ELEVATION }, //! 0: Wire in X direction, pylon on the SW end only
	{ WSO_X_NE,          0,  7, 15,  1,  1, ELRAIL_ELEVATION }, //! 1: Wire in X direction, pylon on the NE end
	{ WSO_X_SHORT,       0,  7, 15,  1,  1, ELRAIL_ELEVATION }, //! 2: Wire in X direction, pylon on both ends

	/* "up" tiles
		 * Wires */
	{ WSO_X_SW_UP,       0,  7, 15,  8,  1, ELRAIL_ELEVRAISE }, //! 3: Wire in X pitch up, pylon on the SW end only
	{ WSO_X_NE_UP,       0,  7, 15,  8,  1, ELRAIL_ELEVRAISE }, //! 4: Wire in X pitch up, pylon on the NE end
	{ WSO_X_SHORT_UP,    0,  7, 15,  8,  1, ELRAIL_ELEVRAISE }, //! 5: Wire in X pitch up, pylon on both ends

	/* "down" tiles
		 * Wires */
	{ WSO_X_SW_DOWN,     0,  7, 15,  8,  1, ELRAIL_ELEVLOWER }, //! 6: Wire in X pitch down, pylon on the SW end
	{ WSO_X_NE_DOWN,     0,  7, 15,  8,  1, ELRAIL_ELEVLOWER }, //! 7: Wire in X pitch down, pylon on the NE end
	{ WSO_X_SHORT_DOWN,  0,  7, 15,  8,  1, ELRAIL_ELEVLOWER }, //! 8: Wire in X pitch down, pylon on both ends


/* Y direction
	 * Flat tiles:
		 * Wires */
	{ WSO_Y_SE,          7,  0,  1, 15,  1, ELRAIL_ELEVATION }, //! 9: Wire in Y direction, pylon on the SE end only
	{ WSO_Y_NW,          7,  0,  1, 15,  1, ELRAIL_ELEVATION }, //!10: Wire in Y direction, pylon on the NW end
	{ WSO_Y_SHORT,       7,  0,  1, 15,  1, ELRAIL_ELEVATION }, //!11: Wire in Y direction, pylon on both ends

	/* "up" tiles
		 * Wires */
	{ WSO_Y_SE_UP,       7,  0,  8, 15,  1, ELRAIL_ELEVRAISE }, //!12: Wire in Y pitch up, pylon on the SE end only
	{ WSO_Y_NW_UP,       7,  0,  8, 15,  1, ELRAIL_ELEVRAISE }, //!13: Wire in Y pitch up, pylon on the NW end
	{ WSO_Y_SHORT_UP,    7,  0,  8, 15,  1, ELRAIL_ELEVRAISE }, //!14: Wire in Y pitch up, pylon on both ends

	/* "down" tiles
		 * Wires */
	{ WSO_Y_SE_DOWN,     7,  0,  8, 15,  1, ELRAIL_ELEVLOWER }, //!15: Wire in Y pitch down, pylon on the SE end
	{ WSO_Y_NW_DOWN,     7,  0,  8, 15,  1, ELRAIL_ELEVLOWER }, //!16: Wire in Y pitch down, pylon on the NW end
	{ WSO_Y_SHORT_DOWN,  7,  0,  8, 15,  1, ELRAIL_ELEVLOWER }, //!17: Wire in Y pitch down, pylon on both ends

/* NS Direction */
	{ WSO_NS_SHORT,      8,  0,  8,  8,  1, ELRAIL_ELEVATION }, //!18: LEFT  trackbit wire, pylon on both ends
	{ WSO_NS_SHORT,      0,  8,  8,  8,  1, ELRAIL_ELEVATION }, //!19: RIGHT trackbit wire, pylon on both ends

	{ WSO_NS_N,          8,  0,  8,  8,  1, ELRAIL_ELEVATION }, //!20: LEFT  trackbit wire, pylon on N end
	{ WSO_NS_N,          0,  8,  8,  8,  1, ELRAIL_ELEVATION }, //!21: RIGHT trackbit wire, pylon on N end

	{ WSO_NS_S,          8,  0,  8,  8,  1, ELRAIL_ELEVATION }, //!22: LEFT  trackbit wire, pylon on S end
	{ WSO_NS_S,          0,  8,  8,  8,  1, ELRAIL_ELEVATION }, //!23: RIGHT trackbit wire, pylon on S end

/* EW Direction */
	{ WSO_EW_SHORT,      7,  0,  1,  1,  1, ELRAIL_ELEVATION }, //!24: UPPER trackbit wire, pylon on both ends
	{ WSO_EW_SHORT,     15,  8,  3,  3,  1, ELRAIL_ELEVATION }, //!25: LOWER trackbit wire, pylon on both ends

	{ WSO_EW_W,          7,  0,  1,  1,  1, ELRAIL_ELEVATION }, //!28: UPPER trackbit wire, pylon on both ends
	{ WSO_EW_W,         15,  8,  3,  3,  1, ELRAIL_ELEVATION }, //!29: LOWER trackbit wire, pylon on both ends

	{ WSO_EW_E,          7,  0,  1,  1,  1, ELRAIL_ELEVATION }, //!32: UPPER trackbit wire, pylon on both ends
	{ WSO_EW_E,         15,  8,  3,  3,  1, ELRAIL_ELEVATION }  //!33: LOWER trackbit wire, pylon on both ends
};

/** Catenary sprite data of a depot for each direction. */
static const DiagDirectionIndexArray<SortableSpriteStruct> _rail_catenary_sprite_data_depot{{{
	{ WSO_ENTRANCE_NE,   0,  7, 15,  1,  1, ELRAIL_ELEVATION }, //! Wire for NE depot exit
	{ WSO_ENTRANCE_SE,   7,  0,  1, 15,  1, ELRAIL_ELEVATION }, //! Wire for SE depot exit
	{ WSO_ENTRANCE_SW,   0,  7, 15,  1,  1, ELRAIL_ELEVATION }, //! Wire for SW depot exit
	{ WSO_ENTRANCE_NW,   7,  0,  1, 15,  1, ELRAIL_ELEVATION }  //! Wire for NW depot exit
}}};

/**
 * In tunnelheads, the bounding box for wires covers nearly the full tile, and is lowered a bit.
 * ELRAIL_TUNNEL_OFFSET is the difference between visual position and bounding box.
 */
static const int8_t ELRAIL_TUNNEL_OFFSET = ELRAIL_ELEVATION - BB_Z_SEPARATOR;

/** Catenary sprite data of a tunnel for each direction. */
static const DiagDirectionIndexArray<SortableSpriteStruct> _rail_catenary_sprite_data_tunnel{{{
	{ WSO_ENTRANCE_SW, {{0, 0, BB_Z_SEPARATOR}, {16, 15, 1}, {0, 7, ELRAIL_TUNNEL_OFFSET}} }, //! Wire for NE tunnel (SW facing exit)
	{ WSO_ENTRANCE_NW, {{0, 0, BB_Z_SEPARATOR}, {15, 16, 1}, {7, 0, ELRAIL_TUNNEL_OFFSET}} }, //! Wire for SE tunnel (NW facing exit)
	{ WSO_ENTRANCE_NE, {{0, 0, BB_Z_SEPARATOR}, {16, 15, 1}, {0, 7, ELRAIL_TUNNEL_OFFSET}} }, //! Wire for SW tunnel (NE facing exit)
	{ WSO_ENTRANCE_SE, {{0, 0, BB_Z_SEPARATOR}, {15, 16, 1}, {7, 0, ELRAIL_TUNNEL_OFFSET}} }  //! Wire for NW tunnel (SE facing exit)
}}};


/**
 * Refers to a certain element of the catenary.
 * Identifiers for Wires:
 * <ol><li>Direction of the wire</li>
 * <li>Slope of the tile for diagonals, placement inside the track for horiz/vertical pieces</li>
 * <li>Place where a pylon should be</li></ol>
 * Identifiers for Pylons:
 * <ol><li>Direction of the wire</li>
 * <li>Slope of the tile</li>
 * <li>Position of the Pylon relative to the track</li>
 * <li>Position of the Pylon inside the tile</li></ol>
 */
enum RailCatenarySprite : uint8_t {
	WIRE_X_FLAT_SW,
	WIRE_X_FLAT_NE,
	WIRE_X_FLAT_BOTH,

	WIRE_X_UP_SW,
	WIRE_X_UP_NE,
	WIRE_X_UP_BOTH,

	WIRE_X_DOWN_SW,
	WIRE_X_DOWN_NE,
	WIRE_X_DOWN_BOTH,

	WIRE_Y_FLAT_SE,
	WIRE_Y_FLAT_NW,
	WIRE_Y_FLAT_BOTH,

	WIRE_Y_UP_SE,
	WIRE_Y_UP_NW,
	WIRE_Y_UP_BOTH,

	WIRE_Y_DOWN_SE,
	WIRE_Y_DOWN_NW,
	WIRE_Y_DOWN_BOTH,

	WIRE_NS_W_BOTH,
	WIRE_NS_E_BOTH,

	WIRE_NS_W_N,
	WIRE_NS_E_N,

	WIRE_NS_W_S,
	WIRE_NS_E_S,

	WIRE_EW_N_BOTH,
	WIRE_EW_S_BOTH,

	WIRE_EW_N_W,
	WIRE_EW_S_W,

	WIRE_EW_N_E,
	WIRE_EW_S_E,

	INVALID_CATENARY = 0xFF
};

/**
 * Selects a Wire (with white and grey ends) depending on whether:
 * a) none (should never happen)
 * b) the first
 * c) the second
 * d) both
 * PCP exists.
 */
static const RailCatenarySprite _rail_wires[5][TRACK_END][4] = {
	{ // Tileh == 0
		{INVALID_CATENARY, WIRE_X_FLAT_NE,   WIRE_X_FLAT_SW,   WIRE_X_FLAT_BOTH},
		{INVALID_CATENARY, WIRE_Y_FLAT_SE,   WIRE_Y_FLAT_NW,   WIRE_Y_FLAT_BOTH},
		{INVALID_CATENARY, WIRE_EW_N_W,      WIRE_EW_N_E,      WIRE_EW_N_BOTH},
		{INVALID_CATENARY, WIRE_EW_S_E,      WIRE_EW_S_W,      WIRE_EW_S_BOTH},
		{INVALID_CATENARY, WIRE_NS_W_S,      WIRE_NS_W_N,      WIRE_NS_W_BOTH},
		{INVALID_CATENARY, WIRE_NS_E_N,      WIRE_NS_E_S,      WIRE_NS_E_BOTH},
	}, { // Tileh == 3
		{INVALID_CATENARY, WIRE_X_UP_NE,     WIRE_X_UP_SW,     WIRE_X_UP_BOTH},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
	}, { // Tileh == 6
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, WIRE_Y_UP_SE,     WIRE_Y_UP_NW,     WIRE_Y_UP_BOTH},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
	}, { // Tileh == 9
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, WIRE_Y_DOWN_SE,   WIRE_Y_DOWN_NW,   WIRE_Y_DOWN_BOTH},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
	}, { // Tileh == 12
		{INVALID_CATENARY, WIRE_X_DOWN_NE,   WIRE_X_DOWN_SW,   WIRE_X_DOWN_BOTH},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
		{INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY, INVALID_CATENARY},
	}
};

#endif /* ELRAIL_DATA_H */
