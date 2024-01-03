/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_base.hpp Base classes for YAPF. */

#ifndef YAPF_BASE_HPP
#define YAPF_BASE_HPP

#include "../../debug.h"
#include "../../settings_type.h"

/**
 * CYapfBaseT - A-star type path finder base class.
 *  Derive your own pathfinder from it. You must provide the following template argument:
 *    Types      - used as collection of local types used in pathfinder
 *
 * Requirements for the Types struct:
 *  ----------------------------------
 *  The following types must be defined in the 'Types' argument:
 *    - Types::Tpf - your pathfinder derived from CYapfBaseT
 *    - Types::NodeList - open/closed node list (look at CNodeList_HashTableT)
 *  NodeList needs to have defined local type Titem - defines the pathfinder node type.
 *  Node needs to define local type Key - the node key in the collection ()
 *
 *  For node list you can use template class CNodeList_HashTableT, for which
 *  you need to declare only your node type. Look at test_yapf.h for an example.
 *
 *
 *  Requirements to your pathfinder class derived from CYapfBaseT:
 *  --------------------------------------------------------------
 *  Your pathfinder derived class needs to implement following methods:
 *    inline void PfSetStartupNodes()
 *    inline void PfFollowNode(Node &org)
 *    inline bool PfCalcCost(Node &n)
 *    inline bool PfCalcEstimate(Node &n)
 *    inline bool PfDetectDestination(Node &n)
 *
 *  For more details about those methods, look at the end of CYapfBaseT
 *  declaration. There are some examples. For another example look at
 *  test_yapf.h (part or unittest project).
 */
template <class Types>
class CYapfBaseT {
public:
	typedef typename Types::Tpf Tpf;           ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::NodeList NodeList; ///< our node list
	typedef typename Types::VehicleType VehicleType; ///< the type of vehicle
	typedef typename NodeList::Titem Node;     ///< this will be our node type
	typedef typename Node::Key Key;            ///< key to hash tables


	NodeList             m_nodes;              ///< node list multi-container
protected:
	Node                *m_pBestDestNode;      ///< pointer to the destination node found at last round
	Node                *m_pBestIntermediateNode; ///< here should be node closest to the destination if path not found
	const YAPFSettings  *m_settings;           ///< current settings (_settings_game.yapf)
	int                  m_max_search_nodes;   ///< maximum number of nodes we are allowed to visit before we give up
	const VehicleType   *m_veh;                ///< vehicle that we are trying to drive

	int                  m_stats_cost_calcs;   ///< stats - how many node's costs were calculated
	int                  m_stats_cache_hits;   ///< stats - how many node's costs were reused from cache

public:
	int                  m_num_steps;          ///< this is there for debugging purposes (hope it doesn't hurt)

public:
	/** default constructor */
	inline CYapfBaseT()
		: m_pBestDestNode(nullptr)
		, m_pBestIntermediateNode(nullptr)
		, m_settings(&_settings_game.pf.yapf)
		, m_max_search_nodes(PfGetSettings().max_search_nodes)
		, m_veh(nullptr)
		, m_stats_cost_calcs(0)
		, m_stats_cache_hits(0)
		, m_num_steps(0)
	{
	}

	/** default destructor */
	~CYapfBaseT() {}

protected:
	/** to access inherited path finder */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** return current settings (can be custom - company based - but later) */
	inline const YAPFSettings &PfGetSettings() const
	{
		return *m_settings;
	}

	/**
	 * Main pathfinder routine:
	 *   - set startup node(s)
	 *   - main loop that stops if:
	 *      - the destination was found
	 *      - or the open list is empty (no route to destination).
	 *      - or the maximum amount of loops reached - m_max_search_nodes (default = 10000)
	 * @return true if the path was found
	 */
	inline bool FindPath(const VehicleType *v)
	{
		m_veh = v;

		Yapf().PfSetStartupNodes();
		bool bDestFound = true;

		for (;;) {
			m_num_steps++;
			Node *n = m_nodes.GetBestOpenNode();
			if (n == nullptr) {
				break;
			}

			/* if the best open node was worse than the best path found, we can finish */
			if (m_pBestDestNode != nullptr && m_pBestDestNode->GetCost() < n->GetCostEstimate()) {
				break;
			}

			Yapf().PfFollowNode(*n);
			if (m_max_search_nodes == 0 || m_nodes.ClosedCount() < m_max_search_nodes) {
				m_nodes.PopOpenNode(n->GetKey());
				m_nodes.InsertClosedNode(*n);
			} else {
				bDestFound = false;
				break;
			}
		}

		bDestFound &= (m_pBestDestNode != nullptr);

		if (_debug_yapf_level >= 3) {
			UnitID veh_idx = (m_veh != nullptr) ? m_veh->unitnumber : 0;
			char ttc = Yapf().TransportTypeChar();
			float cache_hit_ratio = (m_stats_cache_hits == 0) ? 0.0f : ((float)m_stats_cache_hits / (float)(m_stats_cache_hits + m_stats_cost_calcs) * 100.0f);
			int cost = bDestFound ? m_pBestDestNode->m_cost : -1;
			int dist = bDestFound ? m_pBestDestNode->m_estimate - m_pBestDestNode->m_cost : -1;

			Debug(yapf, 3, "[YAPF{}]{}{:4d} - {} rounds - {} open - {} closed - CHR {:4.1f}% - C {} D {}",
				ttc, bDestFound ? '-' : '!', veh_idx, m_num_steps, m_nodes.OpenCount(), m_nodes.ClosedCount(), cache_hit_ratio, cost, dist
			);
		}

		return bDestFound;
	}

	/**
	 * If path was found return the best node that has reached the destination. Otherwise
	 *  return the best visited node (which was nearest to the destination).
	 */
	inline Node *GetBestNode()
	{
		return (m_pBestDestNode != nullptr) ? m_pBestDestNode : m_pBestIntermediateNode;
	}

	/**
	 * Calls NodeList::CreateNewNode() - allocates new node that can be filled and used
	 *  as argument for AddStartupNode() or AddNewNode()
	 */
	inline Node &CreateNewNode()
	{
		Node &node = *m_nodes.CreateNewNode();
		return node;
	}

	/** Add new node (created by CreateNewNode and filled with data) into open list */
	inline void AddStartupNode(Node &n)
	{
		Yapf().PfNodeCacheFetch(n);
		/* insert the new node only if it is not there */
		if (m_nodes.FindOpenNode(n.m_key) == nullptr) {
			m_nodes.InsertOpenNode(n);
		} else {
			/* if we are here, it means that node is already there - how it is possible?
			 *   probably the train is in the position that both its ends point to the same tile/exit-dir
			 *   very unlikely, but it happened */
		}
	}

	/** add multiple nodes - direct children of the given node */
	inline void AddMultipleNodes(Node *parent, const TrackFollower &tf)
	{
		bool is_choice = (KillFirstBit(tf.m_new_td_bits) != TRACKDIR_BIT_NONE);
		for (TrackdirBits rtds = tf.m_new_td_bits; rtds != TRACKDIR_BIT_NONE; rtds = KillFirstBit(rtds)) {
			Trackdir td = (Trackdir)FindFirstBit2x64(rtds);
			Node &n = Yapf().CreateNewNode();
			n.Set(parent, tf.m_new_tile, td, is_choice);
			Yapf().AddNewNode(n, tf);
		}
	}

	/**
	 * In some cases an intermediate node branch should be pruned.
	 * The most prominent case is when a red EOL signal is encountered, but
	 * there was a segment change (e.g. a rail type change) before that. If
	 * the branch would not be pruned, the rail type change location would
	 * remain the best intermediate node, and thus the vehicle would still
	 * go towards the red EOL signal.
	 */
	void PruneIntermediateNodeBranch(Node *n)
	{
		bool intermediate_on_branch = false;
		while (n != nullptr && (n->m_segment->m_end_segment_reason & ESRB_CHOICE_FOLLOWS) == 0) {
			if (n == Yapf().m_pBestIntermediateNode) intermediate_on_branch = true;
			n = n->m_parent;
		}
		if (intermediate_on_branch) Yapf().m_pBestIntermediateNode = n;
	}

	/**
	 * AddNewNode() - called by Tderived::PfFollowNode() for each child node.
	 *  Nodes are evaluated here and added into open list
	 */
	void AddNewNode(Node &n, const TrackFollower &tf)
	{
		/* evaluate the node */
		bool bCached = Yapf().PfNodeCacheFetch(n);
		if (!bCached) {
			m_stats_cost_calcs++;
		} else {
			m_stats_cache_hits++;
		}

		bool bValid = Yapf().PfCalcCost(n, &tf);

		if (bCached) {
			Yapf().PfNodeCacheFlush(n);
		}

		if (bValid) bValid = Yapf().PfCalcEstimate(n);

		/* have the cost or estimate callbacks marked this node as invalid? */
		if (!bValid) return;

		/* detect the destination */
		bool bDestination = Yapf().PfDetectDestination(n);
		if (bDestination) {
			if (m_pBestDestNode == nullptr || n < *m_pBestDestNode) {
				m_pBestDestNode = &n;
			}
			m_nodes.FoundBestNode(n);
			return;
		}

		/* The new node can be set as the best intermediate node only once we're
		 * certain it will be finalized by being inserted into the open list. */
		bool set_intermediate = m_max_search_nodes > 0 && (m_pBestIntermediateNode == nullptr || (m_pBestIntermediateNode->GetCostEstimate() - m_pBestIntermediateNode->GetCost()) > (n.GetCostEstimate() - n.GetCost()));

		/* check new node against open list */
		Node *openNode = m_nodes.FindOpenNode(n.GetKey());
		if (openNode != nullptr) {
			/* another node exists with the same key in the open list
			 * is it better than new one? */
			if (n.GetCostEstimate() < openNode->GetCostEstimate()) {
				/* update the old node by value from new one */
				m_nodes.PopOpenNode(n.GetKey());
				*openNode = n;
				/* add the updated old node back to open list */
				m_nodes.InsertOpenNode(*openNode);
				if (set_intermediate) m_pBestIntermediateNode = openNode;
			}
			return;
		}

		/* check new node against closed list */
		Node *closedNode = m_nodes.FindClosedNode(n.GetKey());
		if (closedNode != nullptr) {
			/* another node exists with the same key in the closed list
			 * is it better than new one? */
			int node_est = n.GetCostEstimate();
			int closed_est = closedNode->GetCostEstimate();
			if (node_est < closed_est) {
				/* If this assert occurs, you have probably problem in
				 * your Tderived::PfCalcCost() or Tderived::PfCalcEstimate().
				 * The problem could be:
				 *  - PfCalcEstimate() gives too large numbers
				 *  - PfCalcCost() gives too small numbers
				 *  - You have used negative cost penalty in some cases (cost bonus) */
				NOT_REACHED();
			}
			return;
		}
		/* the new node is really new
		 * add it to the open list */
		m_nodes.InsertOpenNode(n);
		if (set_intermediate) m_pBestIntermediateNode = &n;
	}

	const VehicleType * GetVehicle() const
	{
		return m_veh;
	}

	void DumpBase(DumpTarget &dmp) const
	{
		dmp.WriteStructT("m_nodes", &m_nodes);
		dmp.WriteValue("m_num_steps", m_num_steps);
	}

	/* methods that should be implemented at derived class Types::Tpf (derived from CYapfBaseT) */

#if 0
	/** Example: PfSetStartupNodes() - set source (origin) nodes */
	inline void PfSetStartupNodes()
	{
		/* example: */
		Node &n1 = *base::m_nodes.CreateNewNode();
		.
		. // setup node members here
		.
		base::m_nodes.InsertOpenNode(n1);
	}

	/** Example: PfFollowNode() - set following (child) nodes of the given node */
	inline void PfFollowNode(Node &org)
	{
		for (each follower of node org) {
			Node &n = *base::m_nodes.CreateNewNode();
			.
			. // setup node members here
			.
			n.m_parent   = &org; // set node's parent to allow back tracking
			AddNewNode(n);
		}
	}

	/** Example: PfCalcCost() - set path cost from origin to the given node */
	inline bool PfCalcCost(Node &n)
	{
		/* evaluate last step cost */
		int cost = ...;
		/* set the node cost as sum of parent's cost and last step cost */
		n.m_cost = n.m_parent->m_cost + cost;
		return true; // true if node is valid follower (i.e. no obstacle was found)
	}

	/** Example: PfCalcEstimate() - set path cost estimate from origin to the target through given node */
	inline bool PfCalcEstimate(Node &n)
	{
		/* evaluate the distance to our destination */
		int distance = ...;
		/* set estimate as sum of cost from origin + distance to the target */
		n.m_estimate = n.m_cost + distance;
		return true; // true if node is valid (i.e. not too far away :)
	}

	/** Example: PfDetectDestination() - return true if the given node is our destination */
	inline bool PfDetectDestination(Node &n)
	{
		bool bDest = (n.m_key.m_x == m_x2) && (n.m_key.m_y == m_y2);
		return bDest;
	}
#endif
};

#endif /* YAPF_BASE_HPP */
