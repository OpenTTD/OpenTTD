/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph.cpp Definition of link graph classes used for cargo distribution. */

#include "../stdafx.h"
#include "../core/pool_func.hpp"
#include "linkgraph.h"

#include "../safeguards.h"

/* Initialize the link-graph-pool */
LinkGraphPool _link_graph_pool("LinkGraph");
INSTANTIATE_POOL_METHODS(LinkGraph)

/**
 * Create a node or clear it.
 * @param xy Location of the associated station.
 * @param st ID of the associated station.
 * @param demand Demand for cargo at the station.
 */
inline void LinkGraph::BaseNode::Init(TileIndex xy, StationID st, uint demand)
{
	this->xy = xy;
	this->supply = 0;
	this->demand = demand;
	this->station = st;
	this->last_update = INVALID_DATE;
}

/**
 * Create an edge.
 */
inline void LinkGraph::BaseEdge::Init()
{
	this->capacity = 0;
	this->usage = 0;
	this->last_unrestricted_update = INVALID_DATE;
	this->last_restricted_update = INVALID_DATE;
	this->next_edge = INVALID_NODE;
}

/**
 * Shift all dates by given interval.
 * This is useful if the date has been modified with the cheat menu.
 * @param interval Number of days to be added or subtracted.
 */
void LinkGraph::ShiftDates(int interval)
{
	this->last_compression += interval;
	for (NodeID node1 = 0; node1 < this->Size(); ++node1) {
		BaseNode &source = this->nodes[node1];
		if (source.last_update != INVALID_DATE) source.last_update += interval;
		for (NodeID node2 = 0; node2 < this->Size(); ++node2) {
			BaseEdge &edge = this->edges[node1][node2];
			if (edge.last_unrestricted_update != INVALID_DATE) edge.last_unrestricted_update += interval;
			if (edge.last_restricted_update != INVALID_DATE) edge.last_restricted_update += interval;
		}
	}
}

void LinkGraph::Compress()
{
	this->last_compression = (_date + this->last_compression) / 2;
	for (NodeID node1 = 0; node1 < this->Size(); ++node1) {
		this->nodes[node1].supply /= 2;
		for (NodeID node2 = 0; node2 < this->Size(); ++node2) {
			BaseEdge &edge = this->edges[node1][node2];
			if (edge.capacity > 0) {
				edge.capacity = max(1U, edge.capacity / 2);
				edge.usage /= 2;
			}
		}
	}
}

/**
 * Merge a link graph with another one.
 * @param other LinkGraph to be merged into this one.
 */
void LinkGraph::Merge(LinkGraph *other)
{
	Date age = _date - this->last_compression + 1;
	Date other_age = _date - other->last_compression + 1;
	NodeID first = this->Size();
	for (NodeID node1 = 0; node1 < other->Size(); ++node1) {
		Station *st = Station::Get(other->nodes[node1].station);
		NodeID new_node = this->AddNode(st);
		this->nodes[new_node].supply = LinkGraph::Scale(other->nodes[node1].supply, age, other_age);
		st->goods[this->cargo].link_graph = this->index;
		st->goods[this->cargo].node = new_node;
		for (NodeID node2 = 0; node2 < node1; ++node2) {
			BaseEdge &forward = this->edges[new_node][first + node2];
			BaseEdge &backward = this->edges[first + node2][new_node];
			forward = other->edges[node1][node2];
			backward = other->edges[node2][node1];
			forward.capacity = LinkGraph::Scale(forward.capacity, age, other_age);
			forward.usage = LinkGraph::Scale(forward.usage, age, other_age);
			if (forward.next_edge != INVALID_NODE) forward.next_edge += first;
			backward.capacity = LinkGraph::Scale(backward.capacity, age, other_age);
			backward.usage = LinkGraph::Scale(backward.usage, age, other_age);
			if (backward.next_edge != INVALID_NODE) backward.next_edge += first;
		}
		BaseEdge &new_start = this->edges[new_node][new_node];
		new_start = other->edges[node1][node1];
		if (new_start.next_edge != INVALID_NODE) new_start.next_edge += first;
	}
	delete other;
}

/**
 * Remove a node from the link graph by overwriting it with the last node.
 * @param id ID of the node to be removed.
 */
void LinkGraph::RemoveNode(NodeID id)
{
	assert(id < this->Size());

	NodeID last_node = this->Size() - 1;
	for (NodeID i = 0; i <= last_node; ++i) {
		(*this)[i].RemoveEdge(id);
		BaseEdge *node_edges = this->edges[i];
		NodeID prev = i;
		NodeID next = node_edges[i].next_edge;
		while (next != INVALID_NODE) {
			if (next == last_node) {
				node_edges[prev].next_edge = id;
				break;
			}
			prev = next;
			next = node_edges[prev].next_edge;
		}
		node_edges[id] = node_edges[last_node];
	}
	Station::Get(this->nodes[last_node].station)->goods[this->cargo].node = id;
	this->nodes.Erase(this->nodes.Get(id));
	this->edges.EraseColumn(id);
	/* Not doing EraseRow here, as having the extra invalid row doesn't hurt
	 * and removing it would trigger a lot of memmove. The data has already
	 * been copied around in the loop above. */
}

/**
 * Add a node to the component and create empty edges associated with it. Set
 * the station's last_component to this component. Calculate the distances to all
 * other nodes. The distances to _all_ nodes are important as the demand
 * calculator relies on their availability.
 * @param st New node's station.
 * @return New node's ID.
 */
NodeID LinkGraph::AddNode(const Station *st)
{
	const GoodsEntry &good = st->goods[this->cargo];

	NodeID new_node = this->Size();
	this->nodes.Append();
	/* Avoid reducing the height of the matrix as that is expensive and we
	 * most likely will increase it again later which is again expensive. */
	this->edges.Resize(new_node + 1U,
			max(new_node + 1U, this->edges.Height()));

	this->nodes[new_node].Init(st->xy, st->index,
			HasBit(good.status, GoodsEntry::GES_ACCEPTANCE));

	BaseEdge *new_edges = this->edges[new_node];

	/* Reset the first edge starting at the new node */
	new_edges[new_node].next_edge = INVALID_NODE;

	for (NodeID i = 0; i <= new_node; ++i) {
		new_edges[i].Init();
		this->edges[i][new_node].Init();
	}
	return new_node;
}

/**
 * Fill an edge with values from a link. Set the restricted or unrestricted
 * update timestamp according to the given update mode.
 * @param to Destination node of the link.
 * @param capacity Capacity of the link.
 * @param usage Usage to be added.
 * @param mode Update mode to be used.
 */
void LinkGraph::Node::AddEdge(NodeID to, uint capacity, uint usage, EdgeUpdateMode mode)
{
	assert(this->index != to);
	BaseEdge &edge = this->edges[to];
	BaseEdge &first = this->edges[this->index];
	edge.capacity = capacity;
	edge.usage = usage;
	edge.next_edge = first.next_edge;
	first.next_edge = to;
	if (mode & EUM_UNRESTRICTED)  edge.last_unrestricted_update = _date;
	if (mode & EUM_RESTRICTED) edge.last_restricted_update = _date;
}

/**
 * Creates an edge if none exists yet or updates an existing edge.
 * @param to Target node.
 * @param capacity Capacity of the link.
 * @param usage Usage to be added.
 * @param mode Update mode to be used.
 */
void LinkGraph::Node::UpdateEdge(NodeID to, uint capacity, uint usage, EdgeUpdateMode mode)
{
	assert(capacity > 0);
	assert(usage <= capacity);
	if (this->edges[to].capacity == 0) {
		this->AddEdge(to, capacity, usage, mode);
	} else {
		(*this)[to].Update(capacity, usage, mode);
	}
}

/**
 * Remove an outgoing edge from this node.
 * @param to ID of destination node.
 */
void LinkGraph::Node::RemoveEdge(NodeID to)
{
	if (this->index == to) return;
	BaseEdge &edge = this->edges[to];
	edge.capacity = 0;
	edge.last_unrestricted_update = INVALID_DATE;
	edge.last_restricted_update = INVALID_DATE;
	edge.usage = 0;

	NodeID prev = this->index;
	NodeID next = this->edges[this->index].next_edge;
	while (next != INVALID_NODE) {
		if (next == to) {
			/* Will be removed, skip it. */
			this->edges[prev].next_edge = edge.next_edge;
			edge.next_edge = INVALID_NODE;
			break;
		} else {
			prev = next;
			next = this->edges[next].next_edge;
		}
	}
}

/**
 * Update an edge. If mode contains UM_REFRESH refresh the edge to have at
 * least the given capacity and usage, otherwise add the capacity and usage.
 * In any case set the respective update timestamp(s), according to the given
 * mode.
 * @param capacity Capacity to be added/updated.
 * @param usage Usage to be added.
 * @param mode Update mode to be applied.
 */
void LinkGraph::Edge::Update(uint capacity, uint usage, EdgeUpdateMode mode)
{
	assert(this->edge.capacity > 0);
	assert(capacity >= usage);

	if (mode & EUM_INCREASE) {
		this->edge.capacity += capacity;
		this->edge.usage += usage;
	} else if (mode & EUM_REFRESH) {
		this->edge.capacity = max(this->edge.capacity, capacity);
		this->edge.usage = max(this->edge.usage, usage);
	}
	if (mode & EUM_UNRESTRICTED) this->edge.last_unrestricted_update = _date;
	if (mode & EUM_RESTRICTED) this->edge.last_restricted_update = _date;
}

/**
 * Resize the component and fill it with empty nodes and edges. Used when
 * loading from save games. The component is expected to be empty before.
 * @param size New size of the component.
 */
void LinkGraph::Init(uint size)
{
	assert(this->Size() == 0);
	this->edges.Resize(size, size);
	this->nodes.Resize(size);

	for (uint i = 0; i < size; ++i) {
		this->nodes[i].Init();
		BaseEdge *column = this->edges[i];
		for (uint j = 0; j < size; ++j) column[j].Init();
	}
}
