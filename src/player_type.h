/* $Id$ */

/** @file player_type.h Types related to players. */

#ifndef PLAYER_TYPE_H
#define PLAYER_TYPE_H

/**
 * Enum for all players/owners.
 */
enum Owner {
	/* Player identifiers All players below MAX_PLAYERS are playable
	 * players, above, they are special, computer controlled players */
	OWNER_BEGIN     = 0x00, ///< First Owner
	PLAYER_FIRST    = 0x00, ///< First Player, same as owner
	MAX_PLAYERS     = 0x08, ///< Maximum numbe rof players
	OWNER_TOWN      = 0x0F, ///< A town owns the tile, or a town is expanding
	OWNER_NONE      = 0x10, ///< The tile has no ownership
	OWNER_WATER     = 0x11, ///< The tile/execution is done by "water"
	OWNER_END,              ///< Last + 1 owner
	INVALID_OWNER   = 0xFF, ///< An invalid owner
	INVALID_PLAYER  = 0xFF, ///< And a valid owner

	/* 'Fake' Players used for networks */
	PLAYER_INACTIVE_CLIENT = 253, ///< The client is joining
	PLAYER_NEW_COMPANY     = 254, ///< The client wants a new company
	PLAYER_SPECTATOR       = 255, ///< The client is spectating
};
DECLARE_POSTFIX_INCREMENT(Owner);

/** Define basic enum properties */
template <> struct EnumPropsT<Owner> : MakeEnumPropsT<Owner, byte, OWNER_BEGIN, OWNER_END, INVALID_OWNER> {};
typedef TinyEnumT<Owner> OwnerByte;

typedef Owner PlayerID;
typedef OwnerByte PlayerByte;

struct Player;
typedef uint32 PlayerFace; ///< player face bits, info see in player_face.h

#endif /* PLAYER_TYPE_H */
