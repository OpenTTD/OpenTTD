/* $Id$ */

/** @file direction.h */

#ifndef DIRECTION_H
#define DIRECTION_H

#include "helpers.hpp"

/**
 * Defines the 8 directions on the map.
 *
 * This enum defines 8 possible directions which are used for
 * the vehicles in the game. The directions are aligned straight
 * to the viewport, not to the map. So north points to the top of
 * your viewport and not rotated by 45 degrees left or right to get
 * a "north" used in you games.
 */
enum Direction {
	DIR_BEGIN = 0,          ///< Used to iterate
	DIR_N   = 0,            ///< North
	DIR_NE  = 1,            ///< Northeast
	DIR_E   = 2,            ///< East
	DIR_SE  = 3,            ///< Southeast
	DIR_S   = 4,            ///< South
	DIR_SW  = 5,            ///< Southwest
	DIR_W   = 6,            ///< West
	DIR_NW  = 7,            ///< Northwest
	DIR_END,                ///< Used to iterate
	INVALID_DIR = 0xFF,     ///< Flag for an invalid direction
};

/** Define basic enum properties */
template <> struct EnumPropsT<Direction> : MakeEnumPropsT<Direction, byte, DIR_BEGIN, DIR_END, INVALID_DIR> {};
typedef TinyEnumT<Direction> DirectionByte; //typedefing-enumification of Direction

/**
 * Return the reverse of a direction
 *
 * @param d The direction to get the reverse from
 * @return The reverse Direction
 */
static inline Direction ReverseDir(Direction d)
{
	return (Direction)(4 ^ d);
}


/**
 * Enumeration for the difference between two directions.
 *
 * This enumeration is used to mark differences between
 * two directions. If you get one direction you can align
 * a second direction in 8 different ways. This enumeration
 * only contains 6 of these 8 differences, but the remaining
 * two can be calculated by adding to differences together.
 * This also means you can add two differences together and
 * get the difference you really want to get. The difference
 * of 45 degrees left + the difference of 45 degrees right results in the
 * difference of 0 degrees.
 *
 * @note To get this mentioned addition of direction you must use
 *       modulo DIR_END or use the #ChangeDirDiff(DirDiff, DirDiff) function.
 * @see ChangeDirDiff(DirDiff, DirDiff)
 */
enum DirDiff {
	DIRDIFF_SAME    = 0,    ///< Both directions faces to the same direction
	DIRDIFF_45RIGHT = 1,    ///< Angle of 45 degrees right
	DIRDIFF_90RIGHT = 2,    ///< Angle of 90 degrees right
	DIRDIFF_REVERSE = 4,    ///< One direction is the opposit of the other one
	DIRDIFF_90LEFT  = 6,    ///< Angle of 90 degrees left
	DIRDIFF_45LEFT  = 7     ///< Angle of 45 degrees left
};

/**
 * Calculate the difference between to directions
 *
 * @param d0 The first direction as the base
 * @param d1 The second direction as the offset from the base
 * @return The difference how the second directions drifts of the first one.
 */
static inline DirDiff DirDifference(Direction d0, Direction d1)
{
	return (DirDiff)((d0 + 8 - d1) % 8);
}

/**
 * Applies two differences together
 *
 * This function adds two differences together and return the resulting
 * difference. So adding two DIRDIFF_REVERSE together results in the
 * DIRDIFF_SAME difference.
 *
 * @param d The first difference
 * @param delta The second difference to add on
 * @return The resulting difference
 */
static inline DirDiff ChangeDirDiff(DirDiff d, DirDiff delta)
{
	return (DirDiff)((d + delta) % 8);
}

/**
 * Change a direction by a given difference
 *
 * This functions returns a new direction of the given direction
 * which is rotated by the given difference.
 *
 * @param d The direction to get a new direction from
 * @param delta The offset/drift applied to the direction
 * @return The new direction
 */
static inline Direction ChangeDir(Direction d, DirDiff delta)
{
	return (Direction)((d + delta) % 8);
}


/**
 * Enumeration for diagonal directions.
 *
 * This enumeration is used for the 4 direction of the tile-edges.
 */
enum DiagDirection {
	DIAGDIR_BEGIN = 0,      ///< Used for iterations
	DIAGDIR_NE  = 0,        ///< Northeast, upper right on your monitor
	DIAGDIR_SE  = 1,        ///< Southeast
	DIAGDIR_SW  = 2,        ///< Southwest
	DIAGDIR_NW  = 3,        ///< Northwest
	DIAGDIR_END,            ///< Used for iterations
	INVALID_DIAGDIR = 0xFF, ///< Flag for an invalid DiagDirection
};

DECLARE_POSTFIX_INCREMENT(DiagDirection);

/** Define basic enum properties */
template <> struct EnumPropsT<DiagDirection> : MakeEnumPropsT<DiagDirection, byte, DIAGDIR_BEGIN, DIAGDIR_END, INVALID_DIAGDIR> {};
typedef TinyEnumT<DiagDirection> DiagDirectionByte; //typedefing-enumification of DiagDirection

/**
 * Returns the reverse direction of the given DiagDirection
 *
 * @param d The DiagDirection to get the reverse from
 * @return The reverse direction
 */
static inline DiagDirection ReverseDiagDir(DiagDirection d)
{
	return (DiagDirection)(2 ^ d);
}

/**
 * Enumeration for the difference between to DiagDirection.
 *
 * As the DiagDirection only contains 4 possible directions the
 * difference between two of these directions can only be in 4 ways.
 * As the DirDiff enumeration the values can be added together and
 * you will get the resulting difference (use modulo DIAGDIR_END).
 *
 * @see DirDiff
 */
enum DiagDirDiff {
	DIAGDIRDIFF_SAME    = 0,        ///< Same directions
	DIAGDIRDIFF_90RIGHT = 1,        ///< 90 degrees right
	DIAGDIRDIFF_REVERSE = 2,        ///< Reverse directions
	DIAGDIRDIFF_90LEFT  = 3         ///< 90 degrees left
};

/** Allow incrementing of DiagDirDiff variables */
DECLARE_POSTFIX_INCREMENT(DiagDirDiff);

/**
 * Applies a difference on a DiagDirection
 *
 * This function applies a difference on a DiagDirection and returns
 * the new DiagDirection.
 *
 * @param d The DiagDirection
 * @param delta The difference to applie on
 * @return The new direction which was calculated
 */
static inline DiagDirection ChangeDiagDir(DiagDirection d, DiagDirDiff delta)
{
	return (DiagDirection)((d + delta) % 4);
}

/**
 * Convert a Direction to a DiagDirection.
 *
 * This function can be used to convert the 8-way Direction to
 * the 4-way DiagDirection. If the direction cannot be mapped its
 * "rounded clockwise". So DIR_N becomes DIAGDIR_NE.
 *
 * @param dir The direction to convert
 * @return The resulting DiagDirection, maybe "rounded clockwise".
 */
static inline DiagDirection DirToDiagDir(Direction dir)
{
	return (DiagDirection)(dir >> 1);
}

/**
 * Convert a DiagDirection to a Direction.
 *
 * This function can be used to convert the 4-way DiagDirection
 * to the 8-way Direction. As 4-way are less than 8-way not all
 * possible directions can be calculated.
 *
 * @param dir The direction to convert
 * @return The resulting Direction
 */
static inline Direction DiagDirToDir(DiagDirection dir)
{
	return (Direction)(dir * 2 + 1);
}


/**
 * Enumeration for the two axis X and Y
 *
 * This enumeration represente the two axis X and Y in the game.
 * The X axis is the one which goes align the north-west edge
 * (and south-east edge). The Y axis must be so the one which goes
 * align the north-east edge (and south-west) edge.
 */
enum Axis {
	AXIS_X = 0,     ///< The X axis
	AXIS_Y = 1,     ///< The y axis
	AXIS_END        ///< Used for iterations
};


/**
 * Select the other axis as provided.
 *
 * This is basically the not-operator for the axis.
 *
 * @param a The given axis
 * @return The other axis
 */
static inline Axis OtherAxis(Axis a)
{
	return (Axis)(a ^ 1);
}


/**
 * Convert a DiagDirection to the axis.
 *
 * This function returns the axis which belongs to the given
 * DiagDirection. The axis X belongs to the DiagDirection
 * north-east and south-west.
 *
 * @param d The DiagDirection
 * @return The axis which belongs to the direction
 */
static inline Axis DiagDirToAxis(DiagDirection d)
{
	return (Axis)(d & 1);
}


/**
 * Converts an Axis to a DiagDirection
 *
 * This function returns the DiagDirection which
 * belongs to the axis. As 2 directions are mapped to an axis
 * this function returns the one which points to south,
 * either south-west (on X axis) or south-east (on Y axis)
 *
 * @param a The axis
 * @return The direction pointed to south
 */
static inline DiagDirection AxisToDiagDir(Axis a)
{
	return (DiagDirection)(2 - a);
}

/**
 * Convert an axis and a flag for north/south into a DiagDirection
 * @param xy axis to convert
 * @param ns north -> 0, south -> 1
 * @return the desired DiagDirection
 */
static inline DiagDirection XYNSToDiagDir(Axis xy, uint ns)
{
	return (DiagDirection)(xy * 3 ^ ns * 2);
}

/**
 * Checks if an interger value is a valid DiagDirection
 *
 * @param d The value to check
 * @return True if the value belongs to a DiagDirection, else false
 */
static inline bool IsValidDiagDirection(DiagDirection d)
{
	return d < DIAGDIR_END;
}

/**
 * Checks if an integer value is a valid Direction
 *
 * @param d The value to check
 * @return True if the value belongs to a Direction, else false
 */
static inline bool IsValidDirection(Direction d)
{
	return d < DIR_END;
}

/**
 * Checks if an integer value is a valid Axis
 *
 * @param d The value to check
 * @return True if the value belongs to an Axis, else false
 */
static inline bool IsValidAxis(Axis d)
{
	return d < AXIS_END;
}

#endif /* DIRECTION_H */
