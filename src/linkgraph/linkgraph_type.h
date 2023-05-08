/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_type.h Declaration of link graph types used for cargo distribution. */

#ifndef LINKGRAPH_TYPE_H
#define LINKGRAPH_TYPE_H

typedef uint16_t LinkGraphID;
static const LinkGraphID INVALID_LINK_GRAPH = UINT16_MAX;

typedef uint16_t LinkGraphJobID;
static const LinkGraphJobID INVALID_LINK_GRAPH_JOB = UINT16_MAX;

typedef uint16_t NodeID;
static const NodeID INVALID_NODE = UINT16_MAX;

enum DistributionType : byte {
	DT_BEGIN = 0,
	DT_MIN = 0,
	DT_MANUAL = 0,           ///< Manual distribution. No link graph calculations are run.
	DT_ASYMMETRIC = 1,       ///< Asymmetric distribution. Usually cargo will only travel in one direction.
	DT_MAX_NONSYMMETRIC = 1, ///< Maximum non-symmetric distribution.
	DT_SYMMETRIC = 2,        ///< Symmetric distribution. The same amount of cargo travels in each direction between each pair of nodes.
	DT_MAX = 2,
	DT_NUM = 3,
	DT_END = 3
};

/**
 * Special modes for updating links. 'Restricted' means that vehicles with
 * 'no loading' orders are serving the link. If a link is only served by
 * such vehicles it's 'fully restricted'. This means the link can be used
 * by cargo arriving in such vehicles, but not by cargo generated or
 * transferring at the source station of the link. In order to find out
 * about this condition we keep two update timestamps in each link, one for
 * the restricted and one for the unrestricted part of it. If either one
 * times out while the other is still valid the link becomes fully
 * restricted or fully unrestricted, respectively.
 * Refreshing a link makes just sure a minimum capacity is kept. Increasing
 * actually adds the given capacity.
 */
enum EdgeUpdateMode {
	EUM_INCREASE     = 1,      ///< Increase capacity.
	EUM_REFRESH      = 1 << 1, ///< Refresh capacity.
	EUM_RESTRICTED   = 1 << 2, ///< Use restricted link.
	EUM_UNRESTRICTED = 1 << 3, ///< Use unrestricted link.
};

DECLARE_ENUM_AS_BIT_SET(EdgeUpdateMode)

#endif /* LINKGRAPH_TYPE_H */
