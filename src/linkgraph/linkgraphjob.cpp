/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraphjob.cpp Definition of link graph job classes used for cargo distribution. */

#include "../stdafx.h"
#include "../core/pool_func.hpp"
#include "../window_func.h"
#include "linkgraphjob.h"
#include "linkgraphschedule.h"

#include "../safeguards.h"

/* Initialize the link-graph-job-pool */
LinkGraphJobPool _link_graph_job_pool("LinkGraphJob");
INSTANTIATE_POOL_METHODS(LinkGraphJob)

/**
 * Static instance of an invalid path.
 * Note: This instance is created on task start.
 *       Lazy creation on first usage results in a data race between the CDist threads.
 */
/* static */ Path *Path::invalid_path = new Path(INVALID_NODE, true);

/**
 * Create a link graph job from a link graph. The link graph will be copied so
 * that the calculations don't interfer with the normal operations on the
 * original. The job is immediately started.
 * @param orig Original LinkGraph to be copied.
 */
LinkGraphJob::LinkGraphJob(const LinkGraph &orig) :
		/* Copying the link graph here also copies its index member.
		 * This is on purpose. */
		link_graph(orig),
		settings(_settings_game.linkgraph),
		join_date(TimerGameCalendar::date + (_settings_game.linkgraph.recalc_time / CalendarTime::SECONDS_PER_DAY)),
		job_completed(false),
		job_aborted(false)
{
}

/**
 * Erase all flows originating at a specific node.
 * @param from Node to erase flows for.
 */
void LinkGraphJob::EraseFlows(NodeID from)
{
	for (NodeID node_id = 0; node_id < this->Size(); ++node_id) {
		(*this)[node_id].flows.erase(from);
	}
}

/**
 * Spawn a thread if possible and run the link graph job in the thread. If
 * that's not possible run the job right now in the current thread.
 */
void LinkGraphJob::SpawnThread()
{
	if (!StartNewThread(&this->thread, "ottd:linkgraph", &(LinkGraphSchedule::Run), this)) {
		/* Of course this will hang a bit.
		 * On the other hand, if you want to play games which make this hang noticeably
		 * on a platform without threads then you'll probably get other problems first.
		 * OK:
		 * If someone comes and tells me that this hangs for them, I'll implement a
		 * smaller grained "Step" method for all handlers and add some more ticks where
		 * "Step" is called. No problem in principle. */
		LinkGraphSchedule::Run(this);
	}
}

/**
 * Join the calling thread with this job's thread if threading is enabled.
 */
void LinkGraphJob::JoinThread()
{
	if (this->thread.joinable()) {
		this->thread.join();
	}
}

/**
 * Join the link graph job and destroy it.
 */
LinkGraphJob::~LinkGraphJob()
{
	this->JoinThread();

	/* Don't update stuff from other pools, when everything is being removed.
	 * Accessing other pools may be invalid. */
	if (CleaningPool()) return;

	/* If the job has been aborted, the job state is invalid.
	 * This should never be reached, as once the job has been marked as aborted
	 * the only valid job operation is to clear the LinkGraphJob pool. */
	assert(!this->IsJobAborted());

	/* Link graph has been merged into another one. */
	if (!LinkGraph::IsValidID(this->link_graph.index)) return;

	uint16_t size = this->Size();
	for (NodeID node_id = 0; node_id < size; ++node_id) {
		NodeAnnotation &from = this->nodes[node_id];

		/* The station can have been deleted. Remove all flows originating from it then. */
		Station *st = Station::GetIfValid(from.base.station);
		if (st == nullptr) {
			this->EraseFlows(node_id);
			continue;
		}

		/* Link graph merging and station deletion may change around IDs. Make
		 * sure that everything is still consistent or ignore it otherwise. */
		GoodsEntry &ge = st->goods[this->Cargo()];
		if (ge.link_graph != this->link_graph.index || ge.node != node_id) {
			this->EraseFlows(node_id);
			continue;
		}

		LinkGraph *lg = LinkGraph::Get(ge.link_graph);
		FlowStatMap &flows = from.flows;

		for (const auto &edge : from.edges) {
			if (edge.Flow() == 0) continue;
			NodeID dest_id = edge.base.dest_node;
			StationID to = this->nodes[dest_id].base.station;
			Station *st2 = Station::GetIfValid(to);
			if (st2 == nullptr || st2->goods[this->Cargo()].link_graph != this->link_graph.index ||
					st2->goods[this->Cargo()].node != dest_id ||
					!(*lg)[node_id].HasEdgeTo(dest_id) ||
					(*lg)[node_id][dest_id].LastUpdate() == CalendarTime::INVALID_DATE) {
				/* Edge has been removed. Delete flows. */
				StationIDStack erased = flows.DeleteFlows(to);
				/* Delete old flows for source stations which have been deleted
				 * from the new flows. This avoids flow cycles between old and
				 * new flows. */
				while (!erased.IsEmpty()) ge.flows.erase(erased.Pop());
			} else if ((*lg)[node_id][dest_id].last_unrestricted_update == CalendarTime::INVALID_DATE) {
				/* Edge is fully restricted. */
				flows.RestrictFlows(to);
			}
		}

		/* Swap shares and invalidate ones that are completely deleted. Don't
		 * really delete them as we could then end up with unroutable cargo
		 * somewhere. Do delete them and also reroute relevant cargo if
		 * automatic distribution has been turned off for that cargo. */
		for (FlowStatMap::iterator it(ge.flows.begin()); it != ge.flows.end();) {
			FlowStatMap::iterator new_it = flows.find(it->first);
			if (new_it == flows.end()) {
				if (_settings_game.linkgraph.GetDistributionType(this->Cargo()) != DT_MANUAL) {
					it->second.Invalidate();
					++it;
				} else {
					FlowStat shares(INVALID_STATION, 1);
					it->second.SwapShares(shares);
					ge.flows.erase(it++);
					for (FlowStat::SharesMap::const_iterator shares_it(shares.GetShares()->begin());
							shares_it != shares.GetShares()->end(); ++shares_it) {
						RerouteCargo(st, this->Cargo(), shares_it->second, st->index);
					}
				}
			} else {
				it->second.SwapShares(new_it->second);
				flows.erase(new_it);
				++it;
			}
		}
		ge.flows.insert(flows.begin(), flows.end());
		InvalidateWindowData(WC_STATION_VIEW, st->index, this->Cargo());
	}
}

/**
 * Initialize the link graph job: Resize nodes and edges and populate them.
 * This is done after the constructor so that we can do it in the calculation
 * thread without delaying the main game.
 */
void LinkGraphJob::Init()
{
	uint size = this->Size();
	this->nodes.reserve(size);
	for (uint i = 0; i < size; ++i) {
		this->nodes.emplace_back(this->link_graph.nodes[i], this->link_graph.Size());
	}
}

/**
 * Add this path as a new child to the given base path, thus making this path
 * a "fork" of the base path.
 * @param base Path to fork from.
 * @param cap Maximum capacity of the new leg.
 * @param free_cap Remaining free capacity of the new leg.
 * @param dist Distance of the new leg.
 */
void Path::Fork(Path *base, uint cap, int free_cap, uint dist)
{
	this->capacity = std::min(base->capacity, cap);
	this->free_capacity = std::min(base->free_capacity, free_cap);
	this->distance = base->distance + dist;
	assert(this->distance > 0);
	if (this->parent != base) {
		this->Detach();
		this->parent = base;
		this->parent->num_children++;
	}
	this->origin = base->origin;
}

/**
 * Push some flow along a path and register the path in the nodes it passes if
 * successful.
 * @param new_flow Amount of flow to push.
 * @param job Link graph job this node belongs to.
 * @param max_saturation Maximum saturation of edges.
 * @return Amount of flow actually pushed.
 */
uint Path::AddFlow(uint new_flow, LinkGraphJob &job, uint max_saturation)
{
	if (this->parent != nullptr) {
		LinkGraphJob::EdgeAnnotation &edge = job[this->parent->node][this->node];
		if (max_saturation != UINT_MAX) {
			uint usable_cap = edge.base.capacity * max_saturation / 100;
			if (usable_cap > edge.Flow()) {
				new_flow = std::min(new_flow, usable_cap - edge.Flow());
			} else {
				return 0;
			}
		}
		new_flow = this->parent->AddFlow(new_flow, job, max_saturation);
		if (this->flow == 0 && new_flow > 0) {
			job[this->parent->node].paths.push_front(this);
		}
		edge.AddFlow(new_flow);
	}
	this->flow += new_flow;
	return new_flow;
}

/**
 * Create a leg of a path in the link graph.
 * @param n Id of the link graph node this path passes.
 * @param source If true, this is the first leg of the path.
 */
Path::Path(NodeID n, bool source) :
	distance(source ? 0 : UINT_MAX),
	capacity(source ? UINT_MAX : 0),
	free_capacity(source ? INT_MAX : INT_MIN),
	flow(0), node(n), origin(source ? n : INVALID_NODE),
	num_children(0), parent(nullptr)
{}

