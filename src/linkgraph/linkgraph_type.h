/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_type.h Declaration of link graph types used for cargo distribution. */

#ifndef LINKGRAPH_TYPE_H
#define LINKGRAPH_TYPE_H

typedef uint16 LinkGraphID;
static const LinkGraphID INVALID_LINK_GRAPH = UINT16_MAX;

typedef uint16 LinkGraphJobID;
static const LinkGraphID INVALID_LIN_KGRAPH_JOB = UINT16_MAX;

typedef uint16 NodeID;
static const NodeID INVALID_NODE = UINT16_MAX;

#endif /* LINKGRAPH_TYPE_H */
