/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file flowmapper.cpp Definition of flowmapper. */

#include "../stdafx.h"
#include "flowmapper.h"

#include "../safeguards.h"

/**
 * Map the paths generated by the MCF solver into flows associated with nodes.
 * @param job the link graph component to be used.
 */
void FlowMapper::Run(LinkGraphJob &job) const
{
	for (NodeID node_id = 0; node_id < job.Size(); ++node_id) {
		Node &prev_node = job[node_id];
		StationID prev = prev_node.base.station;
		for (const Path *path : prev_node.paths) {
			uint flow = path->GetFlow();
			if (flow == 0) break;
			Node &node = job[path->GetNode()];
			StationID via = node.base.station;
			StationID origin = job[path->GetOrigin()].base.station;
			assert(prev != via && via != origin);
			/* Mark all of the flow for local consumption at "first". */
			node.flows.AddFlow(origin, via, flow);
			if (prev != origin) {
				/* Pass some of the flow marked for local consumption at "prev" on
				 * to this node. */
				prev_node.flows.PassOnFlow(origin, via, flow);
			} else {
				/* Prev node is origin. Simply add flow. */
				prev_node.flows.AddFlow(origin, via, flow);
			}
		}
	}

	for (NodeID node_id = 0; node_id < job.Size(); ++node_id) {
		/* Remove local consumption shares marked as invalid. */
		Node &node = job[node_id];
		FlowStatMap &flows = node.flows;
		flows.FinalizeLocalConsumption(node.base.station);
		if (this->scale) {
			/* Scale by time the graph has been running without being compressed. Add 1 to avoid
			 * division by 0 if spawn date == last compression date. This matches
			 * LinkGraph::Monthly(). */
			uint runtime = job.JoinDate() - job.Settings().recalc_time - job.LastCompression() + 1;
			for (auto &it : flows) {
				it.second.ScaleToMonthly(runtime);
			}
		}
		/* Clear paths. */
		for (Path *i : node.paths) delete i;
		node.paths.clear();
	}
}
