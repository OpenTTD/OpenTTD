/* $Id$ */

#ifndef DIRECTION_H
#define DIRECTION_H


/* the 2 axis */
typedef enum Axis {
	AXIS_X = 0,
	AXIS_Y = 1,
	AXIS_END
} Axis;


static inline Axis DiagDirToAxis(uint d)
{
	return (Axis)(d & 1);
}


/*
 * Converts an Axis to a DiagDirection
 * Points always in the positive direction, i.e. S[EW]
 */
static inline uint AxisToDiagDir(Axis a)
{
	return (uint)(2 - a);
}

#endif
