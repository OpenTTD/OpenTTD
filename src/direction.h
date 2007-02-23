/* $Id$ */

/** @file direction.h */

#ifndef DIRECTION_H
#define DIRECTION_H

#include "helpers.hpp"

/* Direction as commonly used in v->direction, 8 way. */
typedef enum Direction {
	DIR_BEGIN = 0,
	DIR_N   = 0,
	DIR_NE  = 1,      ///< Northeast, upper right on your monitor
	DIR_E   = 2,
	DIR_SE  = 3,
	DIR_S   = 4,
	DIR_SW  = 5,
	DIR_W   = 6,
	DIR_NW  = 7,
	DIR_END,
	INVALID_DIR = 0xFF,
} Direction;

/** Define basic enum properties */
template <> struct EnumPropsT<Direction> : MakeEnumPropsT<Direction, byte, DIR_BEGIN, DIR_END, INVALID_DIR> {};
typedef TinyEnumT<Direction> DirectionByte;

static inline Direction ReverseDir(Direction d)
{
	return (Direction)(4 ^ d);
}


typedef enum DirDiff {
	DIRDIFF_SAME    = 0,
	DIRDIFF_45RIGHT = 1,
	DIRDIFF_90RIGHT = 2,
	DIRDIFF_REVERSE = 4,
	DIRDIFF_90LEFT  = 6,
	DIRDIFF_45LEFT  = 7
} DirDiff;

static inline DirDiff DirDifference(Direction d0, Direction d1)
{
	return (DirDiff)((d0 + 8 - d1) % 8);
}

static inline DirDiff ChangeDirDiff(DirDiff d, DirDiff delta)
{
	return (DirDiff)((d + delta) % 8);
}


static inline Direction ChangeDir(Direction d, DirDiff delta)
{
	return (Direction)((d + delta) % 8);
}


/* Direction commonly used as the direction of entering and leaving tiles, 4-way */
typedef enum DiagDirection {
	DIAGDIR_BEGIN = 0,
	DIAGDIR_NE  = 0,      ///< Northeast, upper right on your monitor
	DIAGDIR_SE  = 1,
	DIAGDIR_SW  = 2,
	DIAGDIR_NW  = 3,
	DIAGDIR_END,
	INVALID_DIAGDIR = 0xFF,
} DiagDirection;

DECLARE_POSTFIX_INCREMENT(DiagDirection);

/** Define basic enum properties */
template <> struct EnumPropsT<DiagDirection> : MakeEnumPropsT<DiagDirection, byte, DIAGDIR_BEGIN, DIAGDIR_END, INVALID_DIAGDIR> {};
typedef TinyEnumT<DiagDirection> DiagDirectionByte;

static inline DiagDirection ReverseDiagDir(DiagDirection d)
{
	return (DiagDirection)(2 ^ d);
}


typedef enum DiagDirDiff {
	DIAGDIRDIFF_SAME    = 0,
	DIAGDIRDIFF_90RIGHT = 1,
	DIAGDIRDIFF_REVERSE = 2,
	DIAGDIRDIFF_90LEFT  = 3
} DiagDirDiff;

static inline DiagDirection ChangeDiagDir(DiagDirection d, DiagDirDiff delta)
{
	return (DiagDirection)((d + delta) % 4);
}


static inline DiagDirection DirToDiagDir(Direction dir)
{
	return (DiagDirection)(dir >> 1);
}


static inline Direction DiagDirToDir(DiagDirection dir)
{
	return (Direction)(dir * 2 + 1);
}


/* the 2 axis */
typedef enum Axis {
	AXIS_X = 0,
	AXIS_Y = 1,
	AXIS_END
} Axis;


static inline Axis OtherAxis(Axis a)
{
	return (Axis)(a ^ 1);
}


static inline Axis DiagDirToAxis(DiagDirection d)
{
	return (Axis)(d & 1);
}


/*
 * Converts an Axis to a DiagDirection
 * Points always in the positive direction, i.e. S[EW]
 */
static inline DiagDirection AxisToDiagDir(Axis a)
{
	return (DiagDirection)(2 - a);
}

/**
 * Convert an axis and a flag for north/south into a DiagDirection
 * @param ns north -> 0, south -> 1
 */
static inline DiagDirection XYNSToDiagDir(Axis xy, uint ns)
{
	return (DiagDirection)(xy * 3 ^ ns * 2);
}


static inline bool IsValidDiagDirection(DiagDirection d)
{
	return d < DIAGDIR_END;
}

static inline bool IsValidDirection(Direction d)
{
	return d < DIR_END;
}

static inline bool IsValidAxis(Axis d)
{
	return d < AXIS_END;
}

#endif /* DIRECTION_H */
