/* $Id$ */

#ifndef  YAPF_BASE_HPP
#define  YAPF_BASE_HPP

EXTERN_C_BEGIN
#include "../debug.h"
EXTERN_C_END

#include "fixedsizearray.hpp"
#include "blob.hpp"
#include "nodelist.hpp"

extern int _total_pf_time_us;

/** CYapfBaseT - A-star type path finder base class.
		Derive your own pathfinder from it. You must provide the following template argument:
			Types      - used as collection of local types used in pathfinder

		Requirements for the Types struct:
		----------------------------------
		The following types must be defined in the 'Types' argument:
			- Types::Tpf - your pathfinder derived from CYapfBaseT
			- Types::NodeList - open/closed node list (look at CNodeList_HashTableT)
		NodeList needs to have defined local type Titem - defines the pathfinder node type.
		Node needs to define local type Key - the node key in the collection ()

		For node list you can use template class CNodeList_HashTableT, for which
		you need to declare only your node type. Look at test_yapf.h for an example.


		Requrements to your pathfinder class derived from CYapfBaseT:
		-------------------------------------------------------------
		Your pathfinder derived class needs to implement following methods:
			FORCEINLINE void PfSetStartupNodes()
			FORCEINLINE void PfFollowNode(Node& org)
			FORCEINLINE bool PfCalcCost(Node& n)
			FORCEINLINE bool PfCalcEstimate(Node& n)
			FORCEINLINE bool PfDetectDestination(Node& n)

		For more details about those methods, look at the end of CYapfBaseT
		declaration. There are some examples. For another example look at
		test_yapf.h (part or unittest project).
*/
template <class Types>
class CYapfBaseT {
public:
	typedef typename Types::Tpf Tpf;
	typedef typename Types::NodeList NodeList; ///< our node list
	typedef typename NodeList::Titem Node;     ///< this will be our node type
	typedef typename Node::Key Key;            ///< key to hash tables


	NodeList             m_nodes;         ///< node list multi-container
protected:
	Node*                m_pBestDestNode; ///< pointer to the destination node found at last round
	Node*                m_pBestIntermediateNode;
	const YapfSettings  *m_settings;
	int                  m_max_search_nodes;
	Vehicle*             m_veh;

	int                  m_stats_cost_calcs;
	int                  m_stats_cache_hits;

public:
	CPerformanceTimer    m_perf_cost;
	CPerformanceTimer    m_perf_slope_cost;
	CPerformanceTimer    m_perf_ts_cost;
	CPerformanceTimer    m_perf_other_cost;

public:
	int                  m_num_steps;     ///< this is there for debugging purposes (hope it doesn't hurt)

public:
	// default constructor
	FORCEINLINE CYapfBaseT()
		: m_pBestDestNode(NULL)
		, m_pBestIntermediateNode(NULL)
#if defined(UNITTEST)
		, m_settings(NULL)
		, m_max_search_nodes(100000)
#else
		, m_settings(&_patches.yapf)
		, m_max_search_nodes(PfGetSettings().max_search_nodes)
#endif
		, m_veh(NULL)
		, m_stats_cost_calcs(0)
		, m_stats_cache_hits(0)
		, m_num_steps(0)
	{
	}

	~CYapfBaseT() {}

protected:
	FORCEINLINE Tpf& Yapf() {return *static_cast<Tpf*>(this);}

public:
	FORCEINLINE const YapfSettings& PfGetSettings() const
	{
		return *m_settings;
	}

	/** Main pathfinder routine:
			 - set startup node(s)
			 - main loop that stops if:
					- the destination was found
					- or the open list is empty (no route to destination).
					- or the maximum amount of loops reached - m_max_search_nodes (default = 10000)
			@return true if the path was found */
	inline bool FindPath(Vehicle* v)
	{
		m_veh = v;

		CPerformanceTimer perf;
		perf.Start();
		Yapf().PfSetStartupNodes();

		while (true) {
			m_num_steps++;
			Node& n = m_nodes.GetBestOpenNode();
			if (&n == NULL)
				break;

			// if the best open node was worse than the best path found, we can finish
			if (m_pBestDestNode != NULL && m_pBestDestNode->GetCost() < n.GetCostEstimate())
				break;

			Yapf().PfFollowNode(n);
			if (m_max_search_nodes == 0 || m_nodes.ClosedCount() < m_max_search_nodes) {
				m_nodes.PopOpenNode(n.GetKey());
				m_nodes.InsertClosedNode(n);
			} else {
				m_pBestDestNode = m_pBestIntermediateNode;
				break;
			}
		}
		bool bDestFound = (m_pBestDestNode != NULL);

		int16 veh_idx = (m_veh != NULL) ? m_veh->unitnumber : 0;

//		if (veh_idx != 433) return bDestFound;

		perf.Stop();
		int t = perf.Get(1000000);
		_total_pf_time_us += t;
		char ttc = Yapf().TransportTypeChar();
		float cache_hit_ratio = (float)m_stats_cache_hits / (float)(m_stats_cache_hits + m_stats_cost_calcs) * 100.0f;
		int cost = bDestFound ? m_pBestDestNode->m_cost : -1;
		int dist = bDestFound ? m_pBestDestNode->m_estimate - m_pBestDestNode->m_cost : -1;
#ifdef UNITTEST
		printf("%c%c%4d-%6d us -%5d rounds -%4d open -%5d closed - CHR %4.1f%% - c/d(%d, %d) - c%d(sc%d, ts%d, o%d) -- \n", bDestFound ? '-' : '!', ttc, veh_idx, t, m_num_steps, m_nodes.OpenCount(), m_nodes.ClosedCount(), cache_hit_ratio, cost, dist, m_perf_cost.Get(1000000), m_perf_slope_cost.Get(1000000), m_perf_ts_cost.Get(1000000), m_perf_other_cost.Get(1000000));
#else
		DEBUG(yapf, 1)("[YAPF][YAPF%c]%c%4d- %d us - %d rounds - %d open - %d closed - CHR %4.1f%% - c%d(sc%d, ts%d, o%d) -- ", ttc, bDestFound ? '-' : '!', veh_idx, t, m_num_steps, m_nodes.OpenCount(), m_nodes.ClosedCount(), cache_hit_ratio, cost, dist, m_perf_cost.Get(1000000), m_perf_slope_cost.Get(1000000), m_perf_ts_cost.Get(1000000), m_perf_other_cost.Get(1000000));
#endif
		return bDestFound;
	}

	/** If path was found return the best node that has reached the destination. Otherwise
			return the best visited node (which was nearest to the destination).
	*/
	FORCEINLINE Node& GetBestNode()
	{
		return (m_pBestDestNode != NULL) ? *m_pBestDestNode : *m_pBestIntermediateNode;
	}

	/** Calls NodeList::CreateNewNode() - allocates new node that can be filled and used
			as argument for AddStartupNode() or AddNewNode()
	*/
	FORCEINLINE Node& CreateNewNode()
	{
		Node& node = *m_nodes.CreateNewNode();
		return node;
	}

	/** Add new node (created by CreateNewNode and filled with data) into open list */
	FORCEINLINE void AddStartupNode(Node& n)
	{
		Yapf().PfNodeCacheFetch(n);
		m_nodes.InsertOpenNode(n);
	}

	/** add multiple nodes - direct children of the given node */
	FORCEINLINE void AddMultipleNodes(Node* parent, TileIndex tile, TrackdirBits td_bits)
	{
		for (TrackdirBits rtds = td_bits; rtds != TRACKDIR_BIT_NONE; rtds = (TrackdirBits)KillFirstBit2x64(rtds)) {
			Trackdir td = (Trackdir)FindFirstBit2x64(rtds);
			Node& n = Yapf().CreateNewNode();
			n.Set(parent, tile, td);
			Yapf().AddNewNode(n);
		}
	}

	/** AddNewNode() - called by Tderived::PfFollowNode() for each child node.
	    Nodes are evaluated here and added into open list */
	void AddNewNode(Node& n)
	{
		// evaluate the node
		bool bCached = Yapf().PfNodeCacheFetch(n);
		if (!bCached) {
			m_stats_cost_calcs++;
		} else {
			m_stats_cache_hits++;
		}

		bool bValid = Yapf().PfCalcCost(n);

		if (bCached) {
			Yapf().PfNodeCacheFlush(n);
		}

		if (bValid) bValid = Yapf().PfCalcEstimate(n);

		// have the cost or estimate callbacks marked this node as invalid?
		if (!bValid) return;

		// detect the destination
		bool bDestination = Yapf().PfDetectDestination(n);
		if (bDestination) {
			if (m_pBestDestNode == NULL || n < *m_pBestDestNode) {
				m_pBestDestNode = &n;
			}
			m_nodes.FoundBestNode(n);
			return;
		}

		if (m_max_search_nodes > 0 && (m_pBestIntermediateNode == NULL || (m_pBestIntermediateNode->GetCostEstimate() - m_pBestIntermediateNode->GetCost()) > (n.GetCostEstimate() - n.GetCost()))) {
			m_pBestIntermediateNode = &n;
		}

		// check new node against open list
		Node& openNode = m_nodes.FindOpenNode(n.GetKey());
		if (&openNode != NULL) {
			// another node exists with the same key in the open list
			// is it better than new one?
			if (n.GetCostEstimate() < openNode.GetCostEstimate()) {
				// update the old node by value from new one
				m_nodes.PopOpenNode(n.GetKey());
				openNode = n;
				// add the updated old node back to open list
				m_nodes.InsertOpenNode(openNode);
			}
			return;
		}

		// check new node against closed list
		Node& closedNode = m_nodes.FindClosedNode(n.GetKey());
		if (&closedNode != NULL) {
			// another node exists with the same key in the closed list
			// is it better than new one?
			int node_est = n.GetCostEstimate();
			int closed_est = closedNode.GetCostEstimate();
			if (node_est < closed_est) {
				// If this assert occurs, you have probably problem in
				// your Tderived::PfCalcCost() or Tderived::PfCalcEstimate().
				// The problem could be:
				//  - PfCalcEstimate() gives too large numbers
				//  - PfCalcCost() gives too small numbers
				//  - You have used negative cost penalty in some cases (cost bonus)
				assert(0);

				return;
			}
			return;
		}
		// the new node is really new
		// add it to the open list
		m_nodes.InsertOpenNode(n);
	}

	Vehicle* GetVehicle() const {return m_veh;}

	// methods that should be implemented at derived class Types::Tpf (derived from CYapfBaseT)

#if 0
	/** Example: PfSetStartupNodes() - set source (origin) nodes */
	FORCEINLINE void PfSetStartupNodes()
	{
		// example:
		Node& n1 = *base::m_nodes.CreateNewNode();
		.
		. // setup node members here
		.
		base::m_nodes.InsertOpenNode(n1);
	}

	/** Example: PfFollowNode() - set following (child) nodes of the given node */
	FORCEINLINE void PfFollowNode(Node& org)
	{
		for (each follower of node org) {
			Node& n = *base::m_nodes.CreateNewNode();
			.
			. // setup node members here
			.
			n.m_parent   = &org; // set node's parent to allow back tracking
			AddNewNode(n);
		}
	}

	/** Example: PfCalcCost() - set path cost from origin to the given node */
	FORCEINLINE bool PfCalcCost(Node& n)
	{
		// evaluate last step cost
		int cost = ...;
		// set the node cost as sum of parent's cost and last step cost
		n.m_cost = n.m_parent->m_cost + cost;
		return true; // true if node is valid follower (i.e. no obstacle was found)
	}

	/** Example: PfCalcEstimate() - set path cost estimate from origin to the target through given node */
	FORCEINLINE bool PfCalcEstimate(Node& n)
	{
		// evaluate the distance to our destination
		int distance = ...;
		// set estimate as sum of cost from origin + distance to the target
		n.m_estimate = n.m_cost + distance;
		return true; // true if node is valid (i.e. not too far away :)
	}

	/** Example: PfDetectDestination() - return true if the given node is our destination */
	FORCEINLINE bool PfDetectDestination(Node& n)
	{
		bool bDest = (n.m_key.m_x == m_x2) && (n.m_key.m_y == m_y2);
		return bDest;
	}
#endif
};

#endif /* YAPF_BASE_HPP */
