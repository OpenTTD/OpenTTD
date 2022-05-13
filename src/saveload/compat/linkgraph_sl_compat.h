/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_sl_compat.h Loading of linkgraph chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_LINKGRAPH_H
#define SAVELOAD_COMPAT_LINKGRAPH_H

#include "../saveload.h"

/** Original field order for SlLinkgraphEdge. */
const SaveLoadCompat _linkgraph_edge_sl_compat[] = {
	SLC_NULL(4, SL_MIN_VERSION, SLV_191),
	SLC_VAR("capacity"),
	SLC_VAR("usage"),
	SLC_VAR("last_unrestricted_update"),
	SLC_VAR("last_restricted_update"),
	SLC_VAR("next_edge"),
};

/** Original field order for SlLinkgraphNode. */
const SaveLoadCompat _linkgraph_node_sl_compat[] = {
	SLC_VAR("xy"),
	SLC_VAR("supply"),
	SLC_VAR("demand"),
	SLC_VAR("station"),
	SLC_VAR("last_update"),
	SLC_VAR("edges"),
};

/** Original field order for link_graph_desc. */
const SaveLoadCompat _linkgraph_sl_compat[] = {
	SLC_VAR("last_compression"),
	SLC_VAR("num_nodes"),
	SLC_VAR("cargo"),
	SLC_VAR("nodes"),
};

/** Original field order for job_desc. */
const SaveLoadCompat _linkgraph_job_sl_compat[] = {
	SLC_VAR("linkgraph.recalc_interval"),
	SLC_VAR("linkgraph.recalc_time"),
	SLC_VAR("linkgraph.distribution_pax"),
	SLC_VAR("linkgraph.distribution_mail"),
	SLC_VAR("linkgraph.distribution_armoured"),
	SLC_VAR("linkgraph.distribution_default"),
	SLC_VAR("linkgraph.accuracy"),
	SLC_VAR("linkgraph.demand_distance"),
	SLC_VAR("linkgraph.demand_size"),
	SLC_VAR("linkgraph.short_path_saturation"),
	SLC_VAR("join_date"),
	SLC_VAR("link_graph.index"),
	SLC_VAR("linkgraph"),
};

/** Original field order for schedule_desc. */
const SaveLoadCompat _linkgraph_schedule_sl_compat[] = {
	SLC_VAR("schedule"),
	SLC_VAR("running"),
};

#endif /* SAVELOAD_COMPAT_LINKGRAPH_H */
