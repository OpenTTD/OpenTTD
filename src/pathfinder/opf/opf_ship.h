/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opf_ship.h Original pathfinder for ships; very simple. */

#ifndef OPF_SHIP_H
#define OPF_SHIP_H

#include "../../direction_type.h"

typedef bool TPFEnumProc(TileIndex tile, void *data, Trackdir trackdir, uint length);

void OPFShipFollowTrack(TileIndex tile, DiagDirection direction, TPFEnumProc *enum_proc, void *data);

#endif /* OPF_SHIP_H */
