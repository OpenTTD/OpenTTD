/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file linkgraph_sl.cpp Code handling saving and loading of link graphs */

#include "../stdafx.h"
#include "../linkgraph/linkgraph.h"
#include "../settings_internal.h"
#include "saveload.h"

typedef LinkGraph::BaseNode Node;
typedef LinkGraph::BaseEdge Edge;

static uint _num_nodes;

/**
 * Get a SaveLoad array for a link graph.
 * @return SaveLoad array for link graph.
 */
const SaveLoad *GetLinkGraphDesc()
{
	static const SaveLoad link_graph_desc[] = {
		 SLE_VAR(LinkGraph, last_compression, SLE_UINT32),
		SLEG_VAR(_num_nodes,                  SLE_UINT16),
		 SLE_VAR(LinkGraph, cargo,            SLE_UINT8),
		 SLE_END()
	};
	return link_graph_desc;
}

/* Edges and nodes are saved in the correct order, so we don't need to save their IDs. */

/**
 * SaveLoad desc for a link graph node.
 */
static const SaveLoad _node_desc[] = {
	SLE_VAR(Node, supply,      SLE_UINT32),
	SLE_VAR(Node, demand,      SLE_UINT32),
	SLE_VAR(Node, station,     SLE_UINT16),
	SLE_VAR(Node, last_update, SLE_UINT32),
	SLE_END()
};

/**
 * SaveLoad desc for a link graph edge.
 */
static const SaveLoad _edge_desc[] = {
	SLE_VAR(Edge, distance,    SLE_UINT32),
	SLE_VAR(Edge, capacity,    SLE_UINT32),
	SLE_VAR(Edge, usage,       SLE_UINT32),
	SLE_VAR(Edge, last_update, SLE_UINT32),
	SLE_VAR(Edge, next_edge,   SLE_UINT16),
	SLE_END()
};

/**
 * Save/load a link graph.
 * @param comp Link graph to be saved or loaded.
 */
void SaveLoad_LinkGraph(LinkGraph &lg)
{
	uint size = lg.Size();
	for (NodeID from = 0; from < size; ++from) {
		Node *node = &lg.nodes[from];
		SlObject(node, _node_desc);
		for (NodeID to = 0; to < size; ++to) {
			SlObject(&lg.edges[from][to], _edge_desc);
		}
	}
}

/**
 * Save a link graph.
 * @param lg LinkGraph to be saved.
 */
static void DoSave_LGRP(LinkGraph *lg)
{
	_num_nodes = lg->Size();
	SlObject(lg, GetLinkGraphDesc());
	SaveLoad_LinkGraph(*lg);
}

/**
 * Load all link graphs.
 */
static void Load_LGRP()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		if (!LinkGraph::CanAllocateItem()) {
			/* Impossible as they have been present in previous game. */
			NOT_REACHED();
		}
		LinkGraph *lg = new (index) LinkGraph();
		SlObject(lg, GetLinkGraphDesc());
		lg->Init(_num_nodes);
		SaveLoad_LinkGraph(*lg);
	}
}

/**
 * Save all link graphs.
 */
static void Save_LGRP()
{
	LinkGraph *lg;
	FOR_ALL_LINK_GRAPHS(lg) {
		SlSetArrayIndex(lg->index);
		SlAutolength((AutolengthProc*)DoSave_LGRP, lg);
	}
}

extern const ChunkHandler _linkgraph_chunk_handlers[] = {
	{ 'LGRP', Save_LGRP, Load_LGRP, NULL,      NULL, CH_ARRAY | CH_LAST }
};
