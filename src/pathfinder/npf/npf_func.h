/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file npf_func.h Functions to access the new pathfinder. */

#ifndef NPF_FUNC_H
#define NPF_FUNC_H

Track NPFShipChooseTrack(class Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks);

#endif /* NPF_FUNC_H */
